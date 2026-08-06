/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include <netinet/in.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <atomic>
#include <cstdlib>

#include "errors.h"
#include "optimizer/optimizer.h"
#include "recovery/log_recovery.h"
#include "optimizer/plan.h"
#include "optimizer/planner.h"
#include "portal.h"
#include "analyze/analyze.h"
#include "net/wire.h"

#define SOCK_PORT 8765
#define MAX_CONN_LIMIT 8

static bool should_exit = false;
static std::string database_name;

// 构建全局所需的管理器对象
auto disk_manager = std::make_unique<DiskManager>();
auto buffer_pool_manager = std::make_unique<BufferPoolManager>(BUFFER_POOL_SIZE, disk_manager.get());
auto rm_manager = std::make_unique<RmManager>(disk_manager.get(), buffer_pool_manager.get());
auto ix_manager = std::make_unique<IxManager>(disk_manager.get(), buffer_pool_manager.get());
auto sm_manager = std::make_unique<SmManager>(disk_manager.get(), buffer_pool_manager.get(), rm_manager.get(), ix_manager.get());
auto lock_manager = std::make_unique<LockManager>();
auto txn_manager = std::make_unique<TransactionManager>(lock_manager.get(), sm_manager.get());
auto ql_manager = std::make_unique<QlManager>(sm_manager.get(), txn_manager.get());
auto log_manager = std::make_unique<LogManager>(disk_manager.get());
auto recovery = std::make_unique<RecoveryManager>(disk_manager.get(), buffer_pool_manager.get(), sm_manager.get());
auto planner = std::make_unique<Planner>(sm_manager.get());
auto optimizer = std::make_unique<Optimizer>(sm_manager.get(), planner.get());
auto portal = std::make_unique<Portal>(sm_manager.get());
auto analyze = std::make_unique<Analyze>(sm_manager.get());
pthread_mutex_t *buffer_mutex;

static jmp_buf jmpbuf;
void sigint_handler(int) {
    should_exit = true;
    log_manager->flush_log_to_disk();
    longjmp(jmpbuf, 1);
}

// 判断当前正在执行的是显式事务还是单条SQL语句的事务，并更新事务ID
void SetTransaction(txn_id_t *txn_id, Context *context) {
    context->txn_ = txn_manager->get_transaction(*txn_id);
    if(context->txn_ == nullptr || context->txn_->get_state() == TransactionState::COMMITTED ||
        context->txn_->get_state() == TransactionState::ABORTED) {
        context->txn_ = txn_manager->begin(nullptr, context->log_mgr_);
        *txn_id = context->txn_->get_transaction_id();
        context->txn_->set_txn_mode(false);
    }
}

void *client_handler(void *sock_fd) {
    auto *fd_arg = static_cast<int *>(sock_fd);
    const int fd = *fd_arg;
    delete fd_arg;

    // Fixed-size text buffer still used by RecordPrinter / Context (teaching path).
    // Wire frames wrap that text for EXEC_STREAM responses.
    char *data_send = new char[BUFFER_LENGTH];
    int offset = 0;
    txn_id_t txn_id = INVALID_TXN_ID;

    std::cout << "Client connected (fd=" << fd << ")" << std::endl;

    if (!rucbase::wire::ReadAndCheckHandshake(fd)) {
        std::cerr << "Wire handshake failed (fd=" << fd << ")" << std::endl;
        close(fd);
        delete[] data_send;
        pthread_exit(NULL);
    }

    while (true) {
        rucbase::wire::Frame request;
        if (!rucbase::wire::ReadFrame(fd, &request)) {
            break;
        }
        if (request.flags != 0 || request.tag != rucbase::wire::kTagExecStream) {
            const std::string diag = "unsupported request (only EXEC_STREAM is implemented)";
            if (!rucbase::wire::WriteFrame(fd, rucbase::wire::kTagError, 0, diag)) {
                break;
            }
            continue;
        }

        const std::string &sql = request.payload;
        if (sql.empty()) {
            if (!rucbase::wire::WriteFrame(fd, rucbase::wire::kTagError, 0, "empty SQL")) {
                break;
            }
            continue;
        }

        // Teaching extension retained from the legacy protocol.
        if (sql == rucbase::wire::kDatabaseNameRequest) {
            if (!rucbase::wire::WriteFrame(fd, rucbase::wire::kTagMeta, 0,
                                          rucbase::wire::EncodeMetaSingleCharColumn("database")) ||
                !rucbase::wire::WriteFrame(fd, rucbase::wire::kTagRow, 0,
                                          rucbase::wire::EncodeRowSingleChar(database_name)) ||
                !rucbase::wire::WriteFrame(fd, rucbase::wire::kTagResultEnd, 0,
                                          rucbase::wire::EncodeResultEnd(1))) {
                break;
            }
            continue;
        }

        if (sql == "crash") {
            std::cout << "Server crash" << std::endl;
            exit(1);
        }

        std::cout << "[fd=" << fd << "] " << sql << std::endl;

        memset(data_send, '\0', BUFFER_LENGTH);
        offset = 0;
        enum class Outcome { Ok, Abort, Error };
        Outcome outcome = Outcome::Ok;
        std::string diagnostic;

        Context *context = new Context(lock_manager.get(), log_manager.get(), nullptr, data_send, &offset);
        // Lab 3 need to remove transaction part
        // Lab 4 need to restart transaction
        // SetTransaction(&txn_id, context);

        bool finish_analyze = false;
        pthread_mutex_lock(buffer_mutex);
        YY_BUFFER_STATE buf = yy_scan_string(sql.c_str());
        if (yyparse() == 0) {
            if (ast::parse_tree != nullptr) {
                try {
                    std::shared_ptr<Query> query = analyze->do_analyze(ast::parse_tree);
                    yy_delete_buffer(buf);
                    finish_analyze = true;
                    pthread_mutex_unlock(buffer_mutex);
                    std::shared_ptr<Plan> plan = optimizer->plan_query(query, context);
                    std::shared_ptr<PortalStmt> portalStmt = portal->start(plan, context);
                    portal->run(portalStmt, ql_manager.get(), &txn_id, context);
                    portal->drop();
                } catch (TransactionAbortException &e) {
                    outcome = Outcome::Abort;
                    diagnostic = "abort";
                    txn_manager->abort(context->txn_, log_manager.get());
                    std::cout << e.GetInfo() << std::endl;

                } catch (RMDBError &e) {
                    outcome = Outcome::Error;
                    diagnostic = e.what();
                    if (!diagnostic.empty() && diagnostic.back() != '\n') {
                        diagnostic.push_back('\n');
                    }
                    std::cerr << e.what() << std::endl;

                }
            } else {
                outcome = Outcome::Error;
                diagnostic = "empty parse tree\n";
            }
        } else {
            outcome = Outcome::Error;
            diagnostic = "parse error\n";
        }
        if (finish_analyze == false) {
            yy_delete_buffer(buf);
            pthread_mutex_unlock(buffer_mutex);
        }

        WireResultSet wire_result = std::move(context->wire_result_);
        delete context;

        if (diagnostic.size() > rucbase::wire::kMaxDiagnosticBytes) {
            diagnostic.resize(rucbase::wire::kMaxDiagnosticBytes);
        }

        bool send_ok = true;
        if (outcome == Outcome::Abort) {
            send_ok = rucbase::wire::WriteFrame(fd, rucbase::wire::kTagTransactionAbort, 0, diagnostic);
        } else if (outcome == Outcome::Error) {
            send_ok = rucbase::wire::WriteFrame(fd, rucbase::wire::kTagError, 0, diagnostic);
        } else if (wire_result.has_query_result) {
            // Typed multi-column META / ROW* / RESULT_END (docs/rmdb_wire.md §4.2).
            std::vector<rucbase::wire::ColumnDef> columns;
            columns.reserve(wire_result.columns.size());
            for (const auto &column : wire_result.columns) {
                rucbase::wire::ColumnDef def;
                def.name = column.name;
                def.sql_type = rucbase::wire::ColTypeToWire(static_cast<int>(column.type));
                columns.push_back(std::move(def));
            }
            send_ok = rucbase::wire::WriteFrame(fd, rucbase::wire::kTagMeta, 0,
                                                rucbase::wire::EncodeMeta(columns));
            for (const auto &row : wire_result.rows) {
                if (!send_ok) {
                    break;
                }
                std::vector<rucbase::wire::Cell> cells;
                cells.reserve(row.size());
                for (size_t i = 0; i < row.size(); ++i) {
                    rucbase::wire::Cell cell;
                    cell.sql_type = i < columns.size() ? columns[i].sql_type : rucbase::wire::kTypeChar;
                    const auto &src = row[i];
                    if (cell.sql_type == rucbase::wire::kTypeInt32) {
                        cell.int_val = src.int_val;
                    } else if (cell.sql_type == rucbase::wire::kTypeFloat32) {
                        cell.float_val = src.float_val;
                    } else {
                        cell.str_val = src.str_val;
                    }
                    cells.push_back(std::move(cell));
                }
                send_ok = rucbase::wire::WriteFrame(fd, rucbase::wire::kTagRow, 0,
                                                    rucbase::wire::EncodeRow(cells));
            }
            if (send_ok) {
                send_ok = rucbase::wire::WriteFrame(
                    fd, rucbase::wire::kTagResultEnd, 0,
                    rucbase::wire::EncodeResultEnd(static_cast<uint64_t>(wire_result.rows.size())));
            }
        } else if (offset <= 0) {
            send_ok = rucbase::wire::WriteFrame(fd, rucbase::wire::kTagCommandOk, 0, "");
        } else {
            // help / show tables / desc: still text-formatted into data_send.
            const std::string text(data_send, static_cast<size_t>(offset));
            send_ok =
                rucbase::wire::WriteFrame(fd, rucbase::wire::kTagMeta, 0,
                                          rucbase::wire::EncodeMetaSingleCharColumn("output")) &&
                rucbase::wire::WriteFrame(fd, rucbase::wire::kTagRow, 0,
                                          rucbase::wire::EncodeRowSingleChar(text)) &&
                rucbase::wire::WriteFrame(fd, rucbase::wire::kTagResultEnd, 0,
                                          rucbase::wire::EncodeResultEnd(1));
        }
        if (!send_ok) {
            break;
        }
    }

    std::cout << "Client disconnected (fd=" << fd << ")" << std::endl;
    close(fd);
    delete[] data_send;
    pthread_exit(NULL);
}

void start_server(int server_port) {
    // init mutex
    buffer_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(buffer_mutex, nullptr);

    int sockfd_server;
    int fd_temp;
    struct sockaddr_in s_addr_in {};

    // 初始化连接
    sockfd_server = socket(AF_INET, SOCK_STREAM, 0);  // ipv4,TCP
    assert(sockfd_server != -1);
    int val = 1;
    setsockopt(sockfd_server, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    // before bind(), set the attr of structure sockaddr.
    memset(&s_addr_in, 0, sizeof(s_addr_in));
    s_addr_in.sin_family = AF_INET;
    s_addr_in.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr_in.sin_port = htons(server_port);
    fd_temp = bind(sockfd_server, (struct sockaddr *)(&s_addr_in), sizeof(s_addr_in));
    if (fd_temp == -1) {
        std::cout << "Bind error!" << std::endl;
        exit(1);
    }

    fd_temp = listen(sockfd_server, MAX_CONN_LIMIT);
    if (fd_temp == -1) {
        std::cout << "Listen error!" << std::endl;
        exit(1);
    }

    std::cout << "Rucbase(" << database_name << ") listening on 0.0.0.0:" << server_port << std::endl;

    while (!should_exit) {
        pthread_t thread_id;
        struct sockaddr_in s_addr_client {};
        int client_length = sizeof(s_addr_client);

        if (setjmp(jmpbuf)) {
            break;
        }

        // Block here. Until server accepts a new connection.
        int sockfd = accept(sockfd_server, (struct sockaddr *)(&s_addr_client), (socklen_t *)(&client_length));
        if (sockfd == -1) {
            std::cout << "Accept error!" << std::endl;
            continue;  // ignore current socket ,continue while loop.
        }
        
        // 和客户端建立连接，并开启一个线程负责处理客户端请求
        auto *client_fd = new int(sockfd);
        if (pthread_create(&thread_id, nullptr, &client_handler, client_fd) != 0) {
            std::cout << "Create thread fail!" << std::endl;
            close(sockfd);
            delete client_fd;
            break;  // break while loop
        }
        pthread_detach(thread_id);

    }

    close(sockfd_server);
    sm_manager->close_db();
    std::cout << "Rucbase(" << database_name << ") stopped" << std::endl;
}

int main(int argc, char **argv) {
    int server_port = SOCK_PORT;
    int opt;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        if (opt != 'p') {
            std::cerr << "Usage: " << argv[0] << " [-p port] <database>" << std::endl;
            return 1;
        }

        char *end = nullptr;
        long parsed_port = strtol(optarg, &end, 10);
        if (*optarg == '\0' || *end != '\0' || parsed_port < 1 || parsed_port > 65535) {
            std::cerr << "Invalid port: " << optarg << std::endl;
            return 1;
        }
        server_port = static_cast<int>(parsed_port);
    }

    if (optind != argc - 1) {
        // 需要指定数据库名称
        std::cerr << "Usage: " << argv[0] << " [-p port] <database>" << std::endl;
        exit(1);
    }

    signal(SIGINT, sigint_handler);
    // macOS does not provide MSG_NOSIGNAL. Treat a disconnected peer as an
    // ordinary send() failure instead of allowing SIGPIPE to stop the server.
    signal(SIGPIPE, SIG_IGN);
    try {
        std::cout << "\n"
                     "  ____  _   _  ____ ____    _    ____  _____ \n"
                     " |  _ \\| | | |/ ___| __ )  / \\  / ___|| ____|\n"
                     " | |_) | | | | |   |  _ \\ / _ \\ \\___ \\|  _|  \n"
                     " |  _ <| |_| | |___| |_) / ___ \\ ___) | |___ \n"
                     " |_| \\_ \\___/ \\____|____/_/   \\_\\____/|_____|\n"
                     "\n"
                     "Welcome to Rucbase!\n"
                     "Type 'help;' for help.\n"
                     "\n";
        // Database name is passed by args
        database_name = argv[optind];
        if (!sm_manager->is_dir(database_name)) {
            // Database not found, create a new one
            sm_manager->create_db(database_name);
        }
        // Open database
        sm_manager->open_db(database_name);

        // recovery database
        recovery->analyze();
        recovery->redo();
        recovery->undo();
        
        // 开启服务端，开始接受客户端连接
        start_server(server_port);
    } catch (RMDBError &e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    }
    return 0;
}
