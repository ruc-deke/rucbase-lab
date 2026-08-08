// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "server/server.h"

#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

#include "analyze/analyze.h"
#include "common/config.h"
#include "errors.h"
#include "execution/execution_manager.h"
#include "index/ix_manager.h"
#include "net/wire.h"
#include "optimizer/optimizer.h"
#include "optimizer/planner.h"
#include "parser/parser.h"
#include "portal.h"
#include "record/rm_manager.h"
#include "recovery/log_manager.h"
#include "recovery/log_recovery.h"
#include "storage/buffer_pool_manager.h"
#include "storage/disk_manager.h"
#include "system/sm_manager.h"
#include "transaction/concurrency/lock_manager.h"
#include "transaction/transaction_manager.h"

namespace rucbase {
namespace {

constexpr int kListenBacklog = 64;
constexpr int kListenerPollMilliseconds = 250;

volatile sig_atomic_t g_stop_requested = 0;

void handle_interrupt(int) { g_stop_requested = 1; }

void print_usage(const char* program) {
    std::cerr << "Usage: " << program << " [-b bind_address] [-p port] [-t io_timeout_seconds] <database>\n";
}

bool parse_number(const char* text, const long minimum, const long maximum, long* value) {
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (*text == '\0' || *end != '\0' || parsed < minimum || parsed > maximum) {
        return false;
    }
    *value = parsed;
    return true;
}

bool parse_options(const int argc, char** argv, ServerOptions* options) {
    int option = 0;
    while ((option = ::getopt(argc, argv, "b:p:t:")) != -1) {
        long value = 0;
        switch (option) {
            case 'b':
                options->bind_address = optarg;
                break;
            case 'p':
                if (!parse_number(optarg, 1, 65535, &value)) {
                    std::cerr << "Invalid port: " << optarg << '\n';
                    return false;
                }
                options->port = static_cast<int>(value);
                break;
            case 't':
                if (!parse_number(optarg, 1, 3600, &value)) {
                    std::cerr << "Invalid I/O timeout: " << optarg << '\n';
                    return false;
                }
                options->io_timeout_ms = static_cast<uint32_t>(value) * 1000u;
                break;
            default:
                print_usage(argv[0]);
                return false;
        }
    }

    if (optind != argc - 1) {
        print_usage(argv[0]);
        return false;
    }
    options->database_name = argv[optind];
    return true;
}

std::string errno_message(const std::string& prefix) { return prefix + ": " + std::strerror(errno); }

std::string limit_diagnostic(std::string diagnostic) {
    if (diagnostic.size() > wire::kMaxDiagnosticBytes) {
        diagnostic.resize(wire::kMaxDiagnosticBytes);
    }
    return diagnostic;
}

class Socket {
   public:
    explicit Socket(const int fd = -1) : fd_(fd) {}
    ~Socket() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    [[nodiscard]] int get() const { return fd_; }

    int release() {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }

   private:
    int fd_;
};

}  // namespace

struct Server::ClientSession {
    explicit ClientSession(const int client_fd) : fd(client_fd), text_buffer(BUFFER_LENGTH, '\0') {}

    int fd;
    txn_id_t txn_id = INVALID_TXN_ID;
    std::vector<char> text_buffer;
    int text_offset = 0;
};

struct Server::StatementResult {
    enum class Status { Ok, Abort, Error };

    Status status = Status::Ok;
    std::string diagnostic;
    WireResultSet query_result;
    std::string text;
};

Server::Server(ServerOptions options)
    : options_(std::move(options)),
      buffer_pool_manager_(BUFFER_POOL_SIZE, &disk_manager_),
      rm_manager_(&disk_manager_, &buffer_pool_manager_),
      ix_manager_(&disk_manager_, &buffer_pool_manager_),
      sm_manager_(&disk_manager_, &buffer_pool_manager_, &rm_manager_, &ix_manager_),
      log_manager_(&disk_manager_),
      transaction_manager_(&lock_manager_, &sm_manager_),
      ql_manager_(&sm_manager_, &transaction_manager_),
      recovery_manager_(&disk_manager_, &buffer_pool_manager_, &sm_manager_),
      planner_(&sm_manager_),
      optimizer_(&sm_manager_, &planner_),
      portal_(&sm_manager_),
      analyze_(&sm_manager_) {
    const char* test_crash = std::getenv("RUCBASE_ALLOW_TEST_CRASH");
    allow_test_crash_ = test_crash != nullptr && std::strcmp(test_crash, "1") == 0;
}

int Server::run() {
    int exit_code = 0;
    try {
        install_signal_handlers();
        open_database();
        serve();
    } catch (const RMDBError& error) {
        log_error(error.what());
        exit_code = 1;
    } catch (const std::exception& error) {
        log_error(error.what());
        exit_code = 1;
    } catch (...) {
        log_error("unknown server error");
        exit_code = 1;
    }

    stop_clients();
    if (!close_database()) {
        exit_code = 1;
    }
    if (exit_code == 0) {
        log_info("Rucbase(" + options_.database_name + ") stopped");
    }
    return exit_code;
}

void Server::install_signal_handlers() {
    struct sigaction interrupt_action{};
    interrupt_action.sa_handler = handle_interrupt;
    sigemptyset(&interrupt_action.sa_mask);
    if (::sigaction(SIGINT, &interrupt_action, nullptr) != 0) {
        throw std::runtime_error(errno_message("failed to install SIGINT handler"));
    }

    struct sigaction pipe_action{};
    pipe_action.sa_handler = SIG_IGN;
    sigemptyset(&pipe_action.sa_mask);
    if (::sigaction(SIGPIPE, &pipe_action, nullptr) != 0) {
        throw std::runtime_error(errno_message("failed to ignore SIGPIPE"));
    }
}

void Server::open_database() {
    if (!sm_manager_.is_dir(options_.database_name)) {
        sm_manager_.create_db(options_.database_name);
    }
    sm_manager_.open_db(options_.database_name);
    database_open_ = true;

    recovery_manager_.analyze();
    recovery_manager_.redo();
    recovery_manager_.undo();
}

bool Server::close_database() {
    if (!database_open_) {
        return true;
    }

    bool success = true;
    try {
        log_manager_.flush_log_to_disk();
    } catch (const std::exception& error) {
        log_error("failed to flush log: " + std::string(error.what()));
        success = false;
    }
    try {
        sm_manager_.close_db();
    } catch (const std::exception& error) {
        log_error("failed to close database: " + std::string(error.what()));
        success = false;
    }
    database_open_ = false;
    return success;
}

int Server::create_listening_socket() const {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    addrinfo* addresses = nullptr;
    const std::string port = std::to_string(options_.port);
    if (const int result = ::getaddrinfo(options_.bind_address.c_str(), port.c_str(), &hints, &addresses);
        result != 0) {
        throw std::runtime_error("failed to resolve bind address: " + std::string(::gai_strerror(result)));
    }

    int listener = -1;
    int last_error = EADDRNOTAVAIL;
    for (const addrinfo* address = addresses; address != nullptr; address = address->ai_next) {
        const int fd = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (fd < 0) {
            last_error = errno;
            continue;
        }

        constexpr int reuse_address = 1;
        if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse_address, sizeof(reuse_address)) == 0 &&
            ::bind(fd, address->ai_addr, address->ai_addrlen) == 0 && ::listen(fd, kListenBacklog) == 0) {
            listener = fd;
            break;
        }
        last_error = errno;
        ::close(fd);
    }
    ::freeaddrinfo(addresses);

    if (listener < 0) {
        throw std::runtime_error("failed to listen on " + options_.bind_address + ":" + port + ": " +
                                 std::strerror(last_error));
    }
    return listener;
}

void Server::serve() {
    const Socket listener(create_listening_socket());
    log_info("Rucbase(" + options_.database_name + ") listening on " + options_.bind_address + ":" +
             std::to_string(options_.port));

    while (g_stop_requested == 0) {
        reap_client_threads();

        pollfd descriptor{listener.get(), POLLIN, 0};
        const int poll_result = ::poll(&descriptor, 1, kListenerPollMilliseconds);
        if (poll_result < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error(errno_message("listener poll failed"));
        }
        if (poll_result == 0) {
            continue;
        }
        if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            throw std::runtime_error("listener stopped unexpectedly");
        }
        if ((descriptor.revents & POLLIN) == 0) {
            continue;
        }

        Socket client(::accept(listener.get(), nullptr, nullptr));
        if (client.get() < 0) {
            if (errno != EINTR) {
                log_error(errno_message("accept failed"));
            }
            continue;
        }
        if (!wire::ConfigureConnectedSocket(client.get(), options_.io_timeout_ms)) {
            log_error(errno_message("failed to configure client socket"));
            continue;
        }

        const int client_fd = client.release();
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            client_fds_.insert(client_fd);
        }
        try {
            client_threads_.emplace_back(&Server::handle_client, this, client_fd);
        } catch (const std::system_error& error) {
            close_client(client_fd);
            log_error("failed to create client thread: " + std::string(error.what()));
        }
    }
}

void Server::reap_client_threads() {
    std::vector<std::thread> finished_threads;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto thread = client_threads_.begin();
        while (thread != client_threads_.end()) {
            if (finished_client_threads_.erase(thread->get_id()) == 0) {
                ++thread;
                continue;
            }
            finished_threads.push_back(std::move(*thread));
            thread = client_threads_.erase(thread);
        }
    }

    for (std::thread& thread : finished_threads) {
        thread.join();
    }
}

void Server::stop_clients() {
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (const int fd : client_fds_) {
            ::shutdown(fd, SHUT_RDWR);
        }
    }
    for (std::thread& thread : client_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    client_threads_.clear();
    finished_client_threads_.clear();
}

void Server::close_client(const int fd) {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    ::close(fd);
    client_fds_.erase(fd);
}

void Server::handle_client(const int fd) {
    ClientSession session(fd);
    log_info("Client connected (fd=" + std::to_string(fd) + ")");

    try {
        if (!wire::ReadAndCheckHandshake(fd)) {
            log_error("Wire handshake failed (fd=" + std::to_string(fd) + ")");
        } else {
            while (g_stop_requested == 0) {
                wire::Frame request;
                if (!wire::ReadFrame(fd, &request) || !handle_request(&session, request)) {
                    break;
                }
            }
        }
    } catch (const std::exception& error) {
        log_error("client handler failed (fd=" + std::to_string(fd) + "): " + error.what());
    } catch (...) {
        log_error("client handler failed (fd=" + std::to_string(fd) + ")");
    }

    close_client(fd);
    log_info("Client disconnected (fd=" + std::to_string(fd) + ")");
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        finished_client_threads_.insert(std::this_thread::get_id());
    }
}

bool Server::handle_request(ClientSession* session, const wire::Frame& request) {
    if (request.flags != 0 || request.tag != wire::kTagExecStream) {
        return send_error(session->fd, "unsupported request (only EXEC_STREAM is implemented)");
    }
    if (request.payload.empty()) {
        return send_error(session->fd, "empty SQL");
    }
    if (request.payload == wire::kDatabaseNameRequest) {
        return send_text_result(session->fd, "database", options_.database_name);
    }
    if (request.payload == "crash") {
        if (allow_test_crash_) {
            log_info("Server crash requested by test hook");
            std::_Exit(1);
        }
        return send_error(session->fd, "test crash command is disabled");
    }

    log_info("[fd=" + std::to_string(session->fd) + "] " + request.payload);
    return send_result(session->fd, process_sql(session, request.payload));
}

std::shared_ptr<Query> Server::parse_and_analyze(const std::string& sql, std::string* diagnostic) {
    std::shared_ptr<ast::TreeNode> parse_tree;
    {
        // 目前没有实现可重入，全局只有一个语法解析器，多线程连接会出现严重争抢
        std::lock_guard<std::mutex> lock(parser_mutex_);
        ast::parse_tree.reset();
        YY_BUFFER_STATE buffer = yy_scan_string(sql.c_str());
        if (buffer == nullptr) {
            throw std::runtime_error("failed to allocate parser buffer");
        }

        rucbase::parser::ResetParseError();
        const int parse_result = yyparse();
        parse_tree = ast::parse_tree;
        yy_delete_buffer(buffer);

        if (parse_result != 0) {
            *diagnostic = rucbase::parser::FormatParseError(sql, rucbase::parser::GetParseError());
            return nullptr;
        }
    }

    if (parse_tree == nullptr) {
        *diagnostic = "empty parse tree\n";
        return nullptr;
    }
    return analyze_.do_analyze(std::move(parse_tree));
}

// rucbase处理sql核心流程
Server::StatementResult Server::process_sql(ClientSession* session, const std::string& sql) {
    StatementResult result;
    std::fill(session->text_buffer.begin(), session->text_buffer.end(), '\0');
    session->text_offset = 0;
    Context context(&lock_manager_, &log_manager_, nullptr, session->text_buffer.data(), &session->text_offset);

    try {
        // Lab 3: keep transaction setup disabled.
        // Lab 4: uncomment the marked call below when the handout asks for it.
        // RUCBASE_LAB4_BEGIN_TRANSACTION
        // prepare_transaction(session, &context);

        // 拿到原始的sql语句后，先送入语法解析，解析成功后进行分析
        std::shared_ptr<Query> query = parse_and_analyze(sql, &result.diagnostic);
        if (query == nullptr) {
            result.status = StatementResult::Status::Error;
            return result;
        }

        // The central teaching path: parse/analyze -> plan -> portal -> executor.
        std::shared_ptr<Plan> plan = optimizer_.plan_query(std::move(query), &context);
        std::shared_ptr<PortalStmt> statement = portal_.start(std::move(plan), &context);
        portal_.run(std::move(statement), &ql_manager_, &session->txn_id, &context);
        portal_.drop();

        // Lab 3: keep implicit transaction commit disabled.
        // Lab 4: uncomment the marked block below when the handout asks for it.
        // RUCBASE_LAB4_AUTO_COMMIT
        // if (context.txn_->get_txn_mode() == false) {
        //     transaction_manager_.commit(context.txn_, context.log_mgr_);
        // }

        if (session->text_offset < 0 || static_cast<size_t>(session->text_offset) > session->text_buffer.size()) {
            throw InternalError("legacy result buffer offset is out of range");
        }
        result.query_result = std::move(context.wire_result_);
        if (session->text_offset > 0) {
            result.text.assign(session->text_buffer.data(), static_cast<size_t>(session->text_offset));
        }
    } catch (TransactionAbortException& error) {
        result.status = StatementResult::Status::Abort;
        result.diagnostic = "abort";
        if (context.txn_ != nullptr) {
            try {
                transaction_manager_.abort(context.txn_, &log_manager_);
            } catch (const std::exception& abort_error) {
                log_error("failed to roll back aborted transaction: " + std::string(abort_error.what()));
            }
        }
        log_info(error.GetInfo());
    } catch (const RMDBError& error) {
        result.status = StatementResult::Status::Error;
        result.diagnostic = error.what();
        if (!result.diagnostic.empty() && result.diagnostic.back() != '\n') {
            result.diagnostic.push_back('\n');
        }
        log_error(error.what());
    } catch (const std::exception& error) {
        result.status = StatementResult::Status::Error;
        result.diagnostic = std::string("internal server error: ") + error.what() + "\n";
        log_error(result.diagnostic);
    } catch (...) {
        result.status = StatementResult::Status::Error;
        result.diagnostic = "unknown internal server error\n";
        log_error(result.diagnostic);
    }

    result.diagnostic = limit_diagnostic(std::move(result.diagnostic));
    return result;
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

bool Server::send_result(const int fd, const StatementResult& result) {
    if (result.status == StatementResult::Status::Abort) {
        return wire::WriteFrame(fd, wire::kTagTransactionAbort, 0, result.diagnostic);
    }
    if (result.status == StatementResult::Status::Error) {
        return send_error(fd, result.diagnostic);
    }
    if (result.query_result.has_query_result) {
        return send_query_result(fd, result.query_result);
    }
    if (!result.text.empty()) {
        return send_text_result(fd, "output", result.text);
    }
    return wire::WriteFrame(fd, wire::kTagCommandOk, 0, "");
}

bool Server::send_query_result(const int fd, const WireResultSet& result) {
    std::vector<wire::ColumnDef> columns;
    columns.reserve(result.columns.size());
    for (const auto& [name, type] : result.columns) {
        wire::ColumnDef column;
        column.name = name;
        if (!wire::ColTypeToWire(static_cast<int>(type), &column.sql_type)) {
            return send_error(fd, "query result contains an unsupported column type");
        }
        columns.push_back(std::move(column));
    }

    // Check the table shape before beginning a multi-frame response.
    for (const std::vector<WireResultCell>& row : result.rows) {
        if (row.size() != columns.size()) {
            return send_error(fd, "query result row does not match its schema");
        }
        for (size_t index = 0; index < row.size(); ++index) {
            if (row[index].type != result.columns[index].type) {
                return send_error(fd, "query result cell type does not match its schema");
            }
        }
    }

    std::string diagnostic;
    std::string payload;
    if (!wire::TryEncodeMeta(columns, &payload, &diagnostic)) {
        return send_error(fd, diagnostic);
    }
    if (!wire::WriteFrame(fd, wire::kTagMeta, 0, payload)) {
        return false;
    }

    for (const std::vector<WireResultCell>& row : result.rows) {
        std::vector<wire::Cell> cells;
        cells.reserve(row.size());
        for (size_t index = 0; index < row.size(); ++index) {
            wire::Cell cell;
            cell.sql_type = columns[index].sql_type;
            if (cell.sql_type == wire::kTypeInt32) {
                cell.int_val = static_cast<int32_t>(row[index].int_val);
            } else if (cell.sql_type == wire::kTypeFloat32) {
                cell.float_val = row[index].float_val;
            } else {
                cell.str_val = row[index].str_val;
            }
            cells.push_back(std::move(cell));
        }

        if (!wire::TryEncodeRow(cells, &payload, &diagnostic) || !wire::WriteFrame(fd, wire::kTagRow, 0, payload)) {
            return false;
        }
    }
    return wire::WriteFrame(fd, wire::kTagResultEnd, 0,
                            wire::EncodeResultEnd(static_cast<uint64_t>(result.rows.size())));
}

bool Server::send_text_result(const int fd, const std::string& column_name, const std::string& text) {
    wire::ColumnDef column{.name = column_name, .sql_type = wire::kTypeChar};
    wire::Cell cell;
    cell.sql_type = wire::kTypeChar;
    cell.str_val = text;

    std::string diagnostic;
    std::string meta;
    std::string row;
    if (!wire::TryEncodeMeta({column}, &meta, &diagnostic) || !wire::TryEncodeRow({cell}, &row, &diagnostic)) {
        return send_error(fd, diagnostic);
    }
    return wire::WriteFrame(fd, wire::kTagMeta, 0, meta) && wire::WriteFrame(fd, wire::kTagRow, 0, row) &&
           wire::WriteFrame(fd, wire::kTagResultEnd, 0, wire::EncodeResultEnd(1));
}

bool Server::send_error(const int fd, const std::string& diagnostic) {
    return wire::WriteFrame(fd, wire::kTagError, 0, limit_diagnostic(diagnostic));
}

void Server::log_info(const std::string& message) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    std::cout << message << std::endl;
}

void Server::log_error(const std::string& message) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    std::cerr << message << std::endl;
}

int Server::start(const int argc, char** argv) {
    ServerOptions options;
    if (!parse_options(argc, argv, &options)) {
        return 1;
    }

    g_stop_requested = 0;
    // BufferPoolManager owns a large page array, so the Server must not live on
    // the small main-thread stack.
    const std::unique_ptr<Server> server(new Server(std::move(options)));
    return server->run();
}

}  // namespace rucbase
