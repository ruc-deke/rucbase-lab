// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

/**
 * @file server.h
 * @brief 定义 Rucbase 的顶层服务器及数据库内核组件的组合关系。
 */

#pragma once

#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "analyze/analyze.h"
#include "execution/execution_manager.h"
#include "index/ix_manager.h"
#include "optimizer/optimizer.h"
#include "optimizer/planner.h"
#include "portal/portal.h"
#include "record/rm_manager.h"
#include "recovery/log_manager.h"
#include "recovery/log_recovery.h"
#include "storage/buffer_pool_manager.h"
#include "storage/disk_manager.h"
#include "system/sm_manager.h"
#include "transaction/concurrency/lock_manager.h"
#include "transaction/transaction_manager.h"

class Context;
struct WireResultSet;

namespace rucbase {

namespace wire {
struct Frame;
}

/**
 * @brief 负责数据库初始化、客户端连接管理和 SQL 请求调度。
 *
 * Server 拥有存储、索引、事务、优化与执行模块。每个客户端由一个线程处理，
 * 所有连接共享同一组数据库内核组件。
 */
class Server {
public:
    /**
     * @brief 解析命令行参数并运行服务器。
     * @param argc 命令行参数数量。
     * @param argv 命令行参数；支持 `-b <IPv4>`、`-p <port>` 和数据库名。
     * @return 正常关闭返回 0，初始化、运行或关闭失败返回 1。
     */
    static int start(int argc, char** argv);

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

private:
    /** @brief 保存一个客户端连接的文件描述符、事务 ID 和文本结果缓冲区。 */
    struct ClientSession;

    /**
     * @brief 按依赖顺序构造数据库内核组件。
     * @param database_name 要打开或创建的数据库名。
     * @param bind_address 服务器监听的 IPv4 地址。
     * @param port 服务器监听端口。
     */
    Server(std::string database_name, std::string bind_address, int port);

    /** @return 服务器正常关闭返回 0，运行或资源清理失败返回 1。 */
    int run();

    /**
     * @brief 创建、绑定并监听 IPv4 TCP 套接字。
     * @return 监听套接字的文件描述符。
     * @throws std::runtime_error 套接字创建、地址解析、绑定或监听失败。
     */
    int create_listening_socket() const;

    /** @brief 接受客户端连接，并为每个连接启动一个处理线程。 */
    void serve();

    /** @brief 唤醒所有客户端线程并等待其结束。 */
    void stop_clients();

    /** @brief 关闭连接，并将其从活动连接集合中移除。 */
    void close_client(int fd);

    /** @brief 完成握手并循环处理一个客户端发送的请求。 */
    void handle_client(int fd);

    /**
     * @brief 校验并分派一帧客户端请求。
     * @param session 发出请求的客户端会话。
     * @param request 已从网络读取的请求帧。
     * @return 响应发送成功时返回 true；连接应结束时返回 false。
     */
    bool handle_request(ClientSession* session, const wire::Frame& request);

    /**
     * @brief 执行一条 SQL 的 parse/analyze、plan、portal/executor 完整流程。
     * @param session 当前客户端会话，用于保存事务 ID 和文本结果。
     * @param sql 待执行的单条 SQL 语句。
     * @return 响应完整发送时返回 true，网络或编码失败时返回 false。
     * @note Lab4 需要在此函数中启用两个带锚点的事务教学开关。
     */
    bool execute_sql(ClientSession* session, const std::string& sql);

    /** @brief 为当前语句取得已有事务，或创建一个新的隐式事务。 */
    void prepare_transaction(ClientSession* session, Context* context);

    /** @brief 将结构化查询结果编码为 META、ROW 和 RESULT_END 帧。 */
    static bool send_query_result(int fd, const WireResultSet& result);

    /** @brief 将一段文本编码为单列、单行的结果集。 */
    static bool send_text_result(int fd, const std::string& column_name, const std::string& text);

    /** @brief 截断过长诊断信息，并发送 ERROR 帧。 */
    static bool send_error(int fd, std::string diagnostic);

    /** @name 服务器配置 */
    ///@{
    std::string database_name_;
    std::string bind_address_;
    int port_;
    ///@}

    /** @name 数据库内核组件（按构造依赖顺序排列） */
    ///@{
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
    ///@}

    /** @name 客户端连接状态 */
    ///@{
    // 教学服务器连接数很少：保存线程，在停机时统一唤醒并等待即可。
    std::mutex clients_mutex_;
    std::vector<int> client_fds_;
    std::vector<std::thread> client_threads_;
    ///@}
};

}  // namespace rucbase
