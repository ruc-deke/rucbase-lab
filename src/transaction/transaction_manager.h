#pragma once

#include <atomic>
#include <unordered_map>

#include "transaction.h"
#include "recovery/log_manager.h"
#include "concurrency/lock_manager.h"
#include "system/sm_manager.h"

enum class ConcurrencyMode { TWO_PHASE_LOCKING = 0, BASIC_TO };

class TransactionManager{
   public:
    explicit TransactionManager(LockManager *lock_manager, SmManager *sm_manager,
                                ConcurrencyMode concurrency_mode = ConcurrencyMode::TWO_PHASE_LOCKING) {
        sm_manager_ = sm_manager;
        lock_manager_ = lock_manager;
        concurrency_mode_ = concurrency_mode;
    }

    ~TransactionManager() = default;

    Transaction * Begin(Transaction * txn, LogManager *log_manager);

    void Commit(Transaction * txn, LogManager *log_manager);

    void Abort(Transaction * txn, LogManager *log_manager);

    ConcurrencyMode GetConcurrencyMode() { return concurrency_mode_; }

    void SetConcurrencyMode(ConcurrencyMode concurrency_mode) { concurrency_mode_ = concurrency_mode; }

    LockManager *GetLockManager() { return lock_manager_; }

    /**
     * 获取对应ID的事务指针
     * @param txn_id 事务ID
     * @return 对应事务对象的指针
     */
    Transaction *GetTransaction(txn_id_t txn_id) {
        if(txn_id == INVALID_TXN_ID) return nullptr;

        assert(TransactionManager::txn_map.find(txn_id) != TransactionManager::txn_map.end());

        auto *res = TransactionManager::txn_map[txn_id];
        assert(res != nullptr);
        assert(res->GetThreadId() == std::this_thread::get_id());
        return res;
    }

    // used for test
    inline txn_id_t GetNextTxnId() { return next_txn_id_; }

    // global map of transactions which are running in the system.
    static std::unordered_map<txn_id_t, Transaction *> txn_map;

    /**
     * @brief used for checkpoint
     */
    void BlockAllTransactions();
    void ResumeAllTransactions();

   private:
    ConcurrencyMode concurrency_mode_;      // 事务使用的并发控制算法，目前只需要考虑2PL
                                                  //    Transaction * current_txn_;
    std::atomic<txn_id_t> next_txn_id_{0};  // 用于分发事务ID
    std::atomic<timestamp_t> next_timestamp_{0};    // 用于分发事务时间戳
    SmManager *sm_manager_;
    LockManager *lock_manager_;
};