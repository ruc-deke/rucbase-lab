// Copyright (c) 2023-2026 Renmin University of China
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
#include "net/wire.h"
#include "parser/parser.h"

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
    explicit ClientSession(const int client_fd) : fd(client_fd) { text_buffer.fill('\0'); }

    int fd;
    // 事务身份随连接保留，文本结果缓冲区则在每条 SQL 执行前清空。
    txn_id_t txn_id = INVALID_TXN_ID;
    std::array<char, BUFFER_LENGTH> text_buffer{};
    int text_offset = 0;
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
      ql_manager_(&sm_manager_, &transaction_manager_),
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

    close_client(fd);
    std::cout << "Client disconnected (fd=" << fd << ")\n";
}

bool Server::handle_request(ClientSession* session, const wire::Frame& request) {
    if (request.flags != 0 || request.tag != wire::kTagExecStream) {
        return send_error(session->fd, "unsupported request (only EXEC_STREAM is implemented)");
    }
    if (request.payload.empty()) {
        return send_error(session->fd, "empty SQL");
    }

    std::cout << "[fd=" << session->fd << "] " << request.payload << '\n';
    return execute_sql(session, request.payload);
}

// 一条 SQL 的完整的处理主线：parse/analyze -> plan -> portal/executor。
bool Server::execute_sql(ClientSession* session, const std::string& sql) {
    // 文本结果缓冲区属于单条语句，每次执行前都要重置。
    session->text_buffer.fill('\0');
    session->text_offset = 0;
    // Context 汇集一次语句执行所需的事务、日志、锁和结果状态。
    Context context(&lock_manager_, &log_manager_, nullptr, session->text_buffer.data(), &session->text_offset);

    try {
        // Lab 3 保持关闭；Lab 4 按实验文档启用下面这一行。
        // RUCBASE_LAB4_BEGIN_TRANSACTION
        // prepare_transaction(session, &context);

        auto parse_result = parser::Parse(sql);
        if (!parse_result.ok()) {
            if (!parse_result.error.has_value()) {
                throw InternalError("parser returned neither a statement nor an error");
            }
            return send_error(session->fd, parser::FormatError(sql, parse_result.error.value()));
        }
        auto query = analyzer_.analyze(parse_result.statement);
        auto plan = optimizer_.plan_query(query, &context);
        auto statement = portal_.start(std::move(plan), &context);
        portal_.run(std::move(statement), &ql_manager_, &session->txn_id, &context);

        // Lab 3 保持关闭；Lab 4 按实验文档启用下面这一段。
        // RUCBASE_LAB4_AUTO_COMMIT
        // if (context.txn_->get_txn_mode() == false) {
        //     transaction_manager_.commit(context.txn_, context.log_mgr_);
        // }

        // 查询返回结构化结果，文本型命令返回单列文本，其余命令只返回成功状态。
        if (context.wire_result_.has_query_result) {
            return send_query_result(session->fd, context.wire_result_);
        }
        if (session->text_offset < 0 || session->text_offset > static_cast<int>(session->text_buffer.size())) {
            throw InternalError("result buffer offset is out of range");
        }
        if (session->text_offset > 0) {
            return send_text_result(session->fd, "output",
                                    std::string(session->text_buffer.data(), session->text_offset));
        }
        return wire::WriteFrame(session->fd, wire::kTagCommandOk, 0, "");
    } catch (TransactionAbortException& error) {
        // 事务异常需要先回滚，再发送专用终止帧以保持客户端协议同步。
        if (context.txn_ != nullptr) {
            transaction_manager_.abort(context.txn_, &log_manager_);
        }
        std::cout << error.GetInfo() << '\n';
        return wire::WriteFrame(session->fd, wire::kTagTransactionAbort, 0, "abort");
    } catch (const RMDBError& error) {
        std::cerr << error.what() << '\n';
        return send_error(session->fd, error.what());
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return send_error(session->fd, std::string("internal server error: ") + error.what());
    }
}

void Server::prepare_transaction(ClientSession* session, Context* context) {
    context->txn_ = transaction_manager_.get_transaction(session->txn_id);
    if (context->txn_ == nullptr || context->txn_->get_state() == TransactionState::COMMITTED ||
        context->txn_->get_state() == TransactionState::ABORTED) {
        context->txn_ = transaction_manager_.begin(nullptr, context->log_mgr_);
        session->txn_id = context->txn_->get_transaction_id();
        context->txn_->set_txn_mode(false);
    }
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

bool Server::send_text_result(int fd, const std::string& column_name, const std::string& text) {
    // 原始文本仍封装为单列、单行结果，复用统一的结果集状态机。
    return wire::WriteFrame(fd, wire::kTagMeta, wire::kFlagRawText, wire::EncodeMetaSingleCharColumn(column_name)) &&
           wire::WriteFrame(fd, wire::kTagRow, 0, wire::EncodeRowSingleChar(text)) &&
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
