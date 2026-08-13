// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <deque>
#include <thread>
#include <unordered_set>
#include <utility>

#include "common/config.h"
#include "txn_defs.h"

/**
 * @brief 一条事务。
 *
 * Lab4 作业主要用 write_set() / lock_set() / get_state()。
 * Lab2 串行 B+ 树可以把传入的 Transaction* 当作可空参数忽略。
 */
class Transaction {
public:
    explicit Transaction(txn_id_t txn_id, IsolationLevel isolation_level = IsolationLevel::SERIALIZABLE)
        : isolation_level_(isolation_level),
          thread_id_(std::this_thread::get_id()),
          prev_lsn_(INVALID_LSN),
          txn_id_(txn_id) {}

    [[nodiscard]] txn_id_t get_transaction_id() const noexcept { return txn_id_; }
    [[nodiscard]] std::thread::id get_thread_id() const noexcept { return thread_id_; }

    void set_txn_mode(bool txn_mode) { txn_mode_ = txn_mode; }
    [[nodiscard]] bool get_txn_mode() const noexcept { return txn_mode_; }

    void set_start_ts(timestamp_t start_ts) { start_ts_ = start_ts; }
    [[nodiscard]] timestamp_t get_start_ts() const noexcept { return start_ts_; }

    [[nodiscard]] IsolationLevel get_isolation_level() const noexcept { return isolation_level_; }

    [[nodiscard]] TransactionState get_state() const noexcept { return state_; }
    void set_state(TransactionState state) { state_ = state; }

    [[nodiscard]] lsn_t get_prev_lsn() const noexcept { return prev_lsn_; }
    void set_prev_lsn(lsn_t prev_lsn) { prev_lsn_ = prev_lsn; }

    void append_write_record(WriteRecord record) { write_set_.push_back(std::move(record)); }
    [[nodiscard]] std::deque<WriteRecord>& write_set() noexcept { return write_set_; }
    [[nodiscard]] const std::deque<WriteRecord>& write_set() const noexcept { return write_set_; }

    [[nodiscard]] std::unordered_set<LockDataId>& lock_set() noexcept { return lock_set_; }
    [[nodiscard]] const std::unordered_set<LockDataId>& lock_set() const noexcept { return lock_set_; }

private:
    bool txn_mode_ = false;
    TransactionState state_ = TransactionState::DEFAULT;
    IsolationLevel isolation_level_;
    std::thread::id thread_id_;
    lsn_t prev_lsn_;
    txn_id_t txn_id_;
    timestamp_t start_ts_ = 0;

    std::deque<WriteRecord> write_set_;
    std::unordered_set<LockDataId> lock_set_;
};
