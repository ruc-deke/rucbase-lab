// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <memory>

class LockManager;
class LogManager;
class QueryResultBuilder;
class Transaction;

/**
 * @brief 一条 SQL 的执行上下文。
 *
 * Lab3 / Lab4 作业通常只用：
 *   - transaction() / set_transaction()
 *   - 需要加锁或写日志时再用 lock_manager() / log_manager()
 * result() 是框架发给客户端的缓冲，作业不必调用，也不必打开 wire_result.h。
 */
class Context {
public:
    Context(LockManager* lock_manager, LogManager* log_manager, Transaction* txn);
    ~Context();

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) noexcept;
    Context& operator=(Context&&) noexcept;

    [[nodiscard]] LockManager* lock_manager() const noexcept { return lock_manager_; }
    [[nodiscard]] LogManager* log_manager() const noexcept { return log_manager_; }

    [[nodiscard]] Transaction* transaction() const noexcept { return txn_; }
    void set_transaction(Transaction* txn) noexcept { txn_ = txn; }

    QueryResultBuilder& result() noexcept;
    [[nodiscard]] const QueryResultBuilder& result() const noexcept;

private:
    LockManager* lock_manager_ = nullptr;  ///< 借用，可空。
    LogManager* log_manager_ = nullptr;    ///< 借用，可空。
    Transaction* txn_ = nullptr;           ///< 借用，可空。
    std::unique_ptr<QueryResultBuilder> result_;
};
