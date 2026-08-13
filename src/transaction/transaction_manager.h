// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "common/config.h"
#include "transaction.h"

class LockManager;
class LogManager;
class SmManager;

enum class ConcurrencyMode { TWO_PHASE_LOCKING = 0, BASIC_TO };

class TransactionManager {
public:
    explicit TransactionManager(LockManager* lock_manager,
                                SmManager* sm_manager,
                                ConcurrencyMode concurrency_mode = ConcurrencyMode::TWO_PHASE_LOCKING)
        : concurrency_mode_(concurrency_mode),
          sm_manager_(sm_manager),
          lock_manager_(lock_manager) {}

    /**
     * @brief 创建新事务并由 *this 独占。
     * @return 表中的借用指针，有效期直到 commit/abort 从表中移除。
     */
    Transaction* begin(LogManager* log_manager);

    void commit(Transaction* txn, LogManager* log_manager);

    void abort(Transaction* txn, LogManager* log_manager);

    [[nodiscard]] ConcurrencyMode get_concurrency_mode() const noexcept { return concurrency_mode_; }

    void set_concurrency_mode(ConcurrencyMode concurrency_mode) { concurrency_mode_ = concurrency_mode; }

    [[nodiscard]] LockManager* get_lock_manager() const noexcept { return lock_manager_; }

    /**
     * @brief 按事务 ID 查找事务。
     * @return 表中的借用指针；找不到或 id 无效时返回 nullptr。有效期直到该事务从 txn_map_ 移除。
     */
    [[nodiscard]] Transaction* get_transaction(txn_id_t txn_id) {
        if (txn_id == INVALID_TXN_ID) {
            return nullptr;
        }
        std::unique_lock<std::mutex> lock(latch_);
        const auto it = txn_map_.find(txn_id);
        if (it == txn_map_.end()) {
            return nullptr;
        }
        return it->second.get();
    }

private:
    ConcurrencyMode concurrency_mode_;
    std::atomic<txn_id_t> next_txn_id_{0};
    std::atomic<timestamp_t> next_timestamp_{0};
    std::mutex latch_;
    std::unordered_map<txn_id_t, std::unique_ptr<Transaction>> txn_map_;
    SmManager* sm_manager_;
    LockManager* lock_manager_;
};
