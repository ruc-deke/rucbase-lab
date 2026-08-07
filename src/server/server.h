// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "analyze/analyze.h"
#include "execution/execution_manager.h"
#include "index/ix_manager.h"
#include "net/wire.h"
#include "optimizer/optimizer.h"
#include "optimizer/planner.h"
#include "portal.h"
#include "record/rm_manager.h"
#include "recovery/log_manager.h"
#include "recovery/log_recovery.h"
#include "storage/buffer_pool_manager.h"
#include "storage/disk_manager.h"
#include "system/sm_manager.h"
#include "transaction/concurrency/lock_manager.h"
#include "transaction/transaction_manager.h"

class Context;
class Query;
struct WireResultSet;

namespace rucbase {

struct ServerOptions {
    std::string bind_address = "127.0.0.1";
    int port = 8765;
    uint32_t io_timeout_ms = wire::kDefaultIoTimeoutMs;
    std::string database_name;
};

// RMDB's top-level server. Its members are listed in dependency order so that
// students can see how the database components fit together.
class Server final {
   public:
    static int start(int argc, char** argv);
    ~Server() = default;

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

   private:
    struct ClientSession;
    struct StatementResult;

    explicit Server(ServerOptions options);
    int run();

    static void install_signal_handlers();
    void open_database();
    bool close_database();

    int create_listening_socket() const;
    void serve();
    void reap_client_threads();
    void stop_clients();
    void close_client(int fd);
    void handle_client(int fd);
    bool handle_request(ClientSession* session, const wire::Frame& request);

    std::shared_ptr<Query> parse_and_analyze(const std::string& sql, std::string* diagnostic);
    StatementResult process_sql(ClientSession* session, const std::string& sql);
    void prepare_transaction(ClientSession* session, Context* context);

    static bool send_result(int fd, const StatementResult& result);
    static bool send_query_result(int fd, const WireResultSet& result);
    static bool send_text_result(int fd, const std::string& column_name, const std::string& text);
    static bool send_error(int fd, const std::string& diagnostic);

    void log_info(const std::string& message);
    void log_error(const std::string& message);

    ServerOptions options_;

    DiskManager disk_manager_;
    BufferPoolManager buffer_pool_manager_;
    RmManager rm_manager_;
    IxManager ix_manager_;
    SmManager sm_manager_;
    LockManager lock_manager_;
    LogManager log_manager_;
    TransactionManager transaction_manager_;
    QlManager ql_manager_;
    RecoveryManager recovery_manager_;
    Planner planner_;
    Optimizer optimizer_;
    Portal portal_;
    Analyze analyze_;

    // flex/bison uses global parser state, so parsing itself must be serialized.
    std::mutex parser_mutex_;

    // Each connection has one thread. Finished threads are reaped by the main
    // loop; active threads are joined before the database components above are
    // destroyed.
    std::mutex clients_mutex_;
    std::unordered_set<int> client_fds_;
    std::vector<std::thread> client_threads_;
    std::unordered_set<std::thread::id> finished_client_threads_;

    std::mutex output_mutex_;
    bool database_open_ = false;
    bool allow_test_crash_ = false;
};

}  // namespace rucbase
