#pragma once

#include <mutex>
#include <condition_variable>
#include "transaction/transaction.h"

static const std::string GroupLockModeStr[10] = {"NON_LOCK", "IS", "IX", "S", "X", "SIX"};

class LockManager {
    enum class LockMode { SHARED, EXLUCSIVE, INTENTION_SHARED, INTENTION_EXCLUSIVE, S_IX };
    /**
     * @brief the locks on data
     * NON_LOCK: there is no lock on the data
     * IS : intention shared lock
     * IX : intention exclusive lock
     * S : shared lock
     * X : exclusive lock
     * SIX : shared lock + intention exclusive lock(locks are from the same transaction)
     */
    enum class GroupLockMode { NON_LOCK, IS, IX, S, X, SIX};

    class LockRequest {
    public:
        LockRequest(txn_id_t txn_id, LockMode lock_mode)
            : txn_id_(txn_id), lock_mode_(lock_mode), granted_(false) {}

        txn_id_t txn_id_;
        LockMode lock_mode_;
        bool granted_;
    };

    class LockRequestQueue {
    public:
        // locks on the data
        std::list<LockRequest> request_queue_;      // 位于同一个数据对象上的锁的队列
        // notify lock request
        std::condition_variable cv_;                // 条件变量，用于唤醒request_queue_
        // the group_lock_mode_ decides whether to grant a lock request or not
        GroupLockMode group_lock_mode_ = GroupLockMode::NON_LOCK;   // 当前数据对象上加锁的组模式
        // waiting bit: if there is a lock request which is not granted, the waiting -bit will be true
        bool is_waiting_ = false;
        // upgrading_flag: if there is a lock waiting for upgrading, other transactions that request for upgrading will be aborted
        // (deadlock prevetion)
        bool upgrading_ = false;                    // 当前队列中是否存在一个正在upgrade的锁
        // the number of shared locks
        int shared_lock_num_ = 0;
        // the number of IX locks
        int IX_lock_num_ = 0;
    };

public:
    LockManager() {}

    ~LockManager() {}

    bool LockSharedOnRecord(Transaction *txn, const Rid &rid, int tab_fd);

    bool LockExclusiveOnRecord(Transaction *txn, const Rid &rid, int tab_fd);

    bool LockSharedOnTable(Transaction *txn, int tab_fd);

    bool LockExclusiveOnTable(Transaction *txn, int tab_fd);

    bool LockISOnTable(Transaction *txn, int tab_fd);

    bool LockIXOnTable(Transaction *txn, int tab_fd);

    bool Unlock(Transaction *txn, LockDataId lock_data_id);

private:
    std::mutex latch_;  // 互斥锁，用于锁表的互斥访问
    std::unordered_map<LockDataId, LockRequestQueue> lock_table_;   // 全局锁表
};
