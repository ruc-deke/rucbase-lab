// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

/**
 * @file server.cpp
 * @brief 实现 RUCBase 服务端的启动、连接处理与 SQL 调度流程。
 */

#include "server/server.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

#include "common/config.h"
#include "common/context.h"
#include "common/errors.h"
#include "common/wire_result.h"
#include "net/wire.h"
#include "parser/parser.h"
#include "transaction/transaction.h"

namespace rucbase {
namespace {

constexpr int kDefaultPort = 8765;
constexpr int kListenBacklog = 8;

volatile sig_atomic_t stop_requested = 0;
volatile sig_atomic_t listening_fd = -1;

// 信号处理函数只修改简单状态并关闭套接字，避免在异步信号上下文执行复杂逻辑。
void handle_interrupt(int) {
    stop_requested = 1;
    if (listening_fd >= 0) {
        ::close(listening_fd);  // close() 可以安全地在信号处理函数中调用
        listening_fd = -1;
    }
}

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " [-b ipv4_address] [-p port] <database>\n";
}

bool parse_options(const int argc, char** argv, std::string* database, std::string* address, int* port) {
    *address = "127.0.0.1";
    *port = kDefaultPort;

    int option;
    while ((option = getopt(argc, argv, "b:p:")) != -1) {
        if (option == 'b') {
            *address = optarg;
        } else if (option == 'p') {
            char* end = nullptr;
            const long value = std::strtol(optarg, &end, 10);
            if (*optarg == '\0' || *end != '\0' || value < 1 || value > 65535) {
                std::cerr << "Invalid port: " << optarg << '\n';
                return false;
            }
            *port = static_cast<int>(value);
        } else {
            print_usage(argv[0]);
            return false;
        }
    }
    if (optind != argc - 1) {
        print_usage(argv[0]);
        return false;
    }
    *database = argv[optind];
    return true;
}

}  // namespace

struct Server::ClientSession {
    explicit ClientSession(const int client_fd) : socket_fd(client_fd) {}

    int socket_fd;
    txn_id_t transaction_id = INVALID_TXN_ID;
};

Server::Server(std::string database_name, std::string bind_address, const int port)
    : database_name_(std::move(database_name)),
      bind_address_(std::move(bind_address)),
      port_(port),
      buffer_pool_manager_(BUFFER_POOL_SIZE, &disk_manager_),
      rm_manager_(&disk_manager_, &buffer_pool_manager_),
      index_manager_(&disk_manager_, &buffer_pool_manager_),
      sm_manager_(&disk_manager_, &buffer_pool_manager_, &rm_manager_, &index_manager_),
      log_manager_(&disk_manager_),
      transaction_manager_(&lock_manager_, &sm_manager_),
      ql_manager_(&sm_manager_, &transaction_manager_, &planner_),
      recovery_manager_(&disk_manager_, &buffer_pool_manager_, &sm_manager_),
      planner_(&sm_manager_),
      optimizer_(&sm_manager_, &planner_),
      portal_(&sm_manager_),
      analyzer_(&sm_manager_) {}

int Server::run() {
    int exit_code = 0;
    bool database_open = false;

    try {
        std::signal(SIGINT, handle_interrupt);
        // 客户端提前断开时让写操作返回错误，而不是用 SIGPIPE 终止整个服务端。
        std::signal(SIGPIPE, SIG_IGN);

        if (!sm_manager_.is_dir(database_name_)) {
            sm_manager_.create_db(database_name_);
        }
        sm_manager_.open_db(database_name_);
        database_open = true;

        // 先完成日志回放，再开始接受客户端请求。
        recovery_manager_.analyze();
        recovery_manager_.redo();
        recovery_manager_.undo();
        serve();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        exit_code = 1;
    }

    // 先结束客户端线程，确保关闭数据库时没有请求仍在访问内核组件。
    stop_clients();
    if (database_open) {
        try {
            log_manager_.flush_log_to_disk();
        } catch (const std::exception& error) {
            std::cerr << "Failed to flush log: " << error.what() << '\n';
            exit_code = 1;
        }
        try {
            sm_manager_.close_db();
        } catch (const std::exception& error) {
            std::cerr << "Failed to close database: " << error.what() << '\n';
            exit_code = 1;
        }
    }

    std::cout << "RUCBase(" << database_name_ << ") stopped\n";
    return exit_code;
}

int Server::create_listening_socket() const {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("Failed to create listening socket");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port_));
    if (::inet_pton(AF_INET, bind_address_.c_str(), &address.sin_addr) != 1) {
        ::close(fd);
        throw std::runtime_error("Invalid IPv4 bind address: " + bind_address_);
    }

    // 允许服务端重启后及时重新绑定仍处于 TIME_WAIT 状态的地址。
    constexpr int reuse_address = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(reuse_address)) != 0 ||
        ::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || ::listen(fd, kListenBacklog) != 0) {
        const std::string message =
            "Failed to listen on " + bind_address_ + ":" + std::to_string(port_) + ": " + std::strerror(errno);
        ::close(fd);
        throw std::runtime_error(message);
    }
    return fd;
}

void Server::serve() {
    listening_fd = create_listening_socket();
    std::cout << "RUCBase(" << database_name_ << ") listening on " << bind_address_ << ':' << port_ << '\n';

    // accept 会阻塞；SIGINT 处理函数关闭监听套接字后，循环便能退出。
    while (!stop_requested) {
        const int client_fd = ::accept(listening_fd, nullptr, nullptr);
        if (client_fd < 0) {
            if (stop_requested) {
                break;
            }
            if (errno != EINTR) {
                std::cerr << "Accept failed: " << std::strerror(errno) << '\n';
            }
            continue;
        }
        if (!wire::ConfigureConnectedSocket(client_fd)) {
            ::close(client_fd);
            continue;
        }

        // 先登记连接，停机流程才能唤醒随后可能阻塞在网络读取上的线程。
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            client_fds_.push_back(client_fd);
        }
        try {
            client_threads_.emplace_back(&Server::handle_client, this, client_fd);
        } catch (const std::exception& error) {
            std::cerr << "Failed to create client thread: " << error.what() << '\n';
            close_client(client_fd);
        }
    }

    if (listening_fd >= 0) {
        ::close(listening_fd);
        listening_fd = -1;
    }
}

void Server::stop_clients() {
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        // shutdown 会唤醒阻塞中的读写；套接字最终由各客户端线程关闭。
        for (int fd : client_fds_) {
            ::shutdown(fd, SHUT_RDWR);
        }
    }
    // 在锁外等待，避免客户端线程在 close_client 中等待同一把锁而死锁。
    for (std::thread& thread : client_threads_) {
        thread.join();
    }
}

void Server::close_client(const int fd) {
    std::lock_guard lock(clients_mutex_);
    ::close(fd);
    // C++20：std::erase(container, value) 删除所有等于 value 的元素。
    std::erase(client_fds_, fd);
}

void Server::handle_client(const int fd) {
    ClientSession session(fd);
    std::cout << "Client connected (fd=" << fd << ")\n";

    // 一次握手建立协议版本后，同一连接可以连续处理多条 SQL 请求。
    if (wire::ReadAndCheckHandshake(fd)) {
        wire::Frame request;
        while (wire::ReadFrame(fd, &request) && handle_request(&session, request)) {
        }
    }

    abort_session_transaction(&session);
    close_client(fd);
    std::cout << "Client disconnected (fd=" << fd << ")\n";
}

bool Server::handle_request(ClientSession* session, const wire::Frame& request) {
    if (request.flags != 0 || request.tag != wire::kTagExecStream) {
        return send_error(session->socket_fd, "unsupported request (only EXEC_STREAM is implemented)");
    }
    if (request.payload.empty()) {
        return send_error(session->socket_fd, "empty SQL");
    }

    std::cout << "[fd=" << session->socket_fd << "] " << request.payload << '\n';
    return execute_sql(session, request.payload);
}

bool Server::execute_sql(ClientSession* session, const std::string& sql_text) {
    Context execution_context(&lock_manager_, &log_manager_, nullptr);

    try {
        // Lab 3 保持关闭；Lab 4 按实验文档启用下面这一行。
        // RUCBASE_LAB4_BEGIN_TRANSACTION
        // prepare_transaction(session, &execution_context);

        // 阶段一：语法解析。解析失败属于客户端输入错误，返回带位置信息的诊断。
        const auto parse_result = parser::Parse(sql_text);
        if (!parse_result.ok()) {
            if (!parse_result.error.has_value()) {
                throw InternalError("parser returned neither a statement nor an error");
            }
            cleanup_failed_request(session, &execution_context);
            return send_error(session->socket_fd, parser::FormatError(sql_text, parse_result.error.value()));
        }

        // 阶段二：语义分析。完成表、列和类型绑定，形成语义化查询。
        const auto analyzed_query = analyzer_.analyze(parse_result.statement);

        // 阶段三：计划生成。Optimizer 根据语义化查询构造可执行计划。
        auto execution_plan = optimizer_.plan_query(analyzed_query, &execution_context);

        // 阶段四：语句执行。Portal 构造执行器树，并将语句交由查询层运行。
        auto executable_statement = portal_.start(std::move(execution_plan), &execution_context);
        portal_.run(std::move(executable_statement), &ql_manager_, &session->transaction_id, &execution_context);

        // Lab 3 保持关闭；Lab 4 按实验文档启用下面这一段。
        // RUCBASE_LAB4_AUTO_COMMIT
        // if (execution_context.transaction() != nullptr &&
        //     execution_context.transaction()->get_txn_mode() == false) {
        //     transaction_manager_.commit(execution_context.transaction(), execution_context.log_manager());
        //     finish_session_transaction(session, &execution_context);
        // }

        return send_statement_result(session, execution_context.result().view());
    } catch (const TransactionAbortException& exception) {
        if (execution_context.transaction() != nullptr) {
            transaction_manager_.abort(execution_context.transaction(), &log_manager_);
            finish_session_transaction(session, &execution_context);
        }
        std::cout << exception.info() << '\n';
        return wire::WriteFrame(session->socket_fd, wire::kTagTransactionAbort, 0, "abort");
    } catch (const RMDBError& exception) {
        cleanup_failed_request(session, &execution_context);
        std::cerr << exception.what() << '\n';
        return send_error(session->socket_fd, exception.what());
    } catch (const std::exception& exception) {
        cleanup_failed_request(session, &execution_context);
        std::cerr << exception.what() << '\n';
        return send_error(session->socket_fd, std::string("internal server error: ") + exception.what());
    }
}

void Server::prepare_transaction(ClientSession* session, Context* context) {
    context->set_transaction(transaction_manager_.get_transaction(session->transaction_id));
    if (context->transaction() == nullptr || context->transaction()->get_state() == TransactionState::COMMITTED ||
        context->transaction()->get_state() == TransactionState::ABORTED) {
        context->set_transaction(transaction_manager_.begin(context->log_manager()));
        session->transaction_id = context->transaction()->get_transaction_id();
        context->transaction()->set_txn_mode(false);
    }
}

void Server::finish_session_transaction(ClientSession* session, Context* context) {
    context->set_transaction(nullptr);
    session->transaction_id = INVALID_TXN_ID;
}

void Server::cleanup_failed_request(ClientSession* session, Context* context) {
    Transaction* txn = context->transaction();
    if (txn == nullptr) {
        txn = transaction_manager_.get_transaction(session->transaction_id);
    }
    if (txn == nullptr || txn->get_txn_mode()) {
        return;
    }
    transaction_manager_.abort(txn, &log_manager_);
    finish_session_transaction(session, context);
}

void Server::abort_session_transaction(ClientSession* session) {
    Transaction* txn = transaction_manager_.get_transaction(session->transaction_id);
    if (txn == nullptr) {
        session->transaction_id = INVALID_TXN_ID;
        return;
    }
    transaction_manager_.abort(txn, &log_manager_);
    session->transaction_id = INVALID_TXN_ID;
}

bool Server::send_statement_result(ClientSession* session, const WireResultSet& structured_result) {
    if (structured_result.has_query_result && structured_result.raw_text) {
        return send_raw_text_result(session->socket_fd, structured_result);
    }
    if (structured_result.has_query_result) {
        return send_query_result(session->socket_fd, structured_result);
    }
    return wire::WriteFrame(session->socket_fd, wire::kTagCommandOk, 0, "");
}

bool Server::send_query_result(int fd, const WireResultSet& result) {
    std::vector<wire::ColumnDef> columns;
    for (const auto& [name, type] : result.columns) {
        wire::ColumnDef column{.name = name};
        if (!wire::ColTypeToWire(static_cast<int>(type), &column.sql_type)) {
            return send_error(fd, "unsupported query result type");
        }
        columns.push_back(std::move(column));
    }

    // 在发送 META 前检查表格形状，避免把不完整的响应写到网络上。
    for (const auto& row : result.rows) {
        if (row.size() != columns.size()) {
            return send_error(fd, "query result row does not match its schema");
        }
        for (size_t i = 0; i < row.size(); ++i) {
            if (row[i].type != result.columns[i].type) {
                return send_error(fd, "query result cell type does not match its schema");
            }
        }
    }

    std::string payload;
    std::string diagnostic;
    // 结果集按 META、ROW*、RESULT_END 的顺序编码，客户端据此重建表格。
    if (!wire::TryEncodeMeta(columns, &payload, &diagnostic)) {
        return send_error(fd, diagnostic);
    }
    if (!wire::WriteFrame(fd, wire::kTagMeta, 0, payload)) {
        return false;
    }

    for (const auto& row : result.rows) {
        std::vector<wire::Cell> cells;
        for (size_t i = 0; i < row.size(); ++i) {
            wire::Cell cell{.sql_type = columns[i].sql_type,
                            .int_val = row[i].int_val,
                            .float_val = row[i].float_val,
                            .str_val = row[i].str_val};
            cells.push_back(std::move(cell));
        }
        if (!wire::TryEncodeRow(cells, &payload, &diagnostic) || !wire::WriteFrame(fd, wire::kTagRow, 0, payload)) {
            return false;
        }
    }
    return wire::WriteFrame(fd, wire::kTagResultEnd, 0, wire::EncodeResultEnd(result.rows.size()));
}

bool Server::send_raw_text_result(int fd, const WireResultSet& result) {
    if (result.columns.size() != 1 || result.rows.size() != 1 || result.rows.front().size() != 1) {
        return send_error(fd, "raw text result must contain one CHAR cell");
    }
    const std::string& column_name = result.columns.front().name;
    const std::string& text = result.rows.front().front().str_val;
    std::string meta;
    std::string row;
    std::string diagnostic;
    const wire::ColumnDef column{.name = column_name, .sql_type = wire::kTypeChar};
    const wire::Cell cell{.sql_type = wire::kTypeChar, .str_val = text};
    if (!wire::TryEncodeMeta({column}, &meta, &diagnostic) || !wire::TryEncodeRow({cell}, &row, &diagnostic)) {
        return send_error(fd, diagnostic.empty() ? "failed to encode raw text result" : diagnostic);
    }
    return wire::WriteFrame(fd, wire::kTagMeta, wire::kFlagRawText, meta) &&
           wire::WriteFrame(fd, wire::kTagRow, 0, row) &&
           wire::WriteFrame(fd, wire::kTagResultEnd, 0, wire::EncodeResultEnd(1));
}

bool Server::send_error(int fd, std::string diagnostic) {
    // 统一补换行并限制长度，避免诊断信息生成过大的协议帧。
    if (!diagnostic.empty() && diagnostic.back() != '\n') {
        diagnostic.push_back('\n');
    }
    if (diagnostic.size() > wire::kMaxDiagnosticBytes) {
        diagnostic.resize(wire::kMaxDiagnosticBytes);
    }
    return wire::WriteFrame(fd, wire::kTagError, 0, diagnostic);
}

int Server::start(int argc, char** argv) {
    std::string database;
    std::string address;
    int port;
    if (!parse_options(argc, argv, &database, &address, &port)) {
        return 1;
    }

    stop_requested = 0;
    // BufferPoolManager 含有较大的页数组，因此 Server 放在堆上。
    const std::unique_ptr<Server> server(new Server(std::move(database), std::move(address), port));
    return server->run();
}

}  // namespace rucbase
