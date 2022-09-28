#pragma once

#include <atomic>
#include <deque>
#include <string>
#include <thread>
#include <unordered_set>

#include "txn_defs.h"

class Transaction {
   public:
    explicit Transaction(txn_id_t txn_id, IsolationLevel isolation_level = IsolationLevel::SERIALIZABLE)
        : state_(TransactionState::DEFAULT), isolation_level_(isolation_level), txn_id_(txn_id) {
        write_set_ = std::make_shared<std::deque<WriteRecord *>>();
        lock_set_ = std::make_shared<std::unordered_set<LockDataId>>();
        page_set_ = std::make_shared<std::deque<Page *>>();
        deleted_page_set_ = std::make_shared<std::deque<Page *>>();
        prev_lsn_ = INVALID_LSN;
        thread_id_ = std::this_thread::get_id();
    }

    ~Transaction() = default;

    inline txn_id_t GetTransactionId() { return txn_id_; }

    inline std::thread::id GetThreadId() { return thread_id_; }

    inline void SetTxnMode(bool txn_mode) { txn_mode_ = txn_mode; }
    inline bool GetTxnMode() { return txn_mode_; }

    inline void SetStartTs(timestamp_t start_ts) { start_ts_ = start_ts; }
    inline timestamp_t GetStartTs() { return start_ts_; }

    inline IsolationLevel GetIsolationLevel() { return isolation_level_; }

    inline TransactionState GetState() { return state_; }
    inline void SetState(TransactionState state) { state_ = state; }

    inline lsn_t GetPrevLsn() { return prev_lsn_; }
    inline void SetPrevLsn(lsn_t prev_lsn) { prev_lsn_ = prev_lsn; }

    inline std::shared_ptr<std::deque<WriteRecord *>> GetWriteSet() { return write_set_; }

    /**
     * @brief 向write_set_中添加写操作
     * @param write_record 要添加的写操作
     */
    inline void AppendWriteRecord(WriteRecord *write_record) { write_set_->push_back(write_record); }

    inline std::shared_ptr<std::unordered_set<LockDataId>> GetLockSet() { return lock_set_; }

    /** @return the page set */
    inline std::shared_ptr<std::deque<Page *>> GetPageSet() {
        return page_set_;
    }

    /**
     * Adds a page into the page set.
     * @param page page to be added
     */
    inline void AddIntoPageSet(Page *page) { page_set_->push_back(page); }

    /** @return the deleted page set */
    inline std::shared_ptr<std::deque<Page *>> GetDeletedPageSet() { return deleted_page_set_; }

    /**
     * Adds a page to the deleted page set.
     * @param page page to be marked as deleted
     */
    inline void AddIntoDeletedPageSet(Page *page) { deleted_page_set_->push_back(page); }

   private:
    bool txn_mode_;  // 用于标识当前事务是否还包含未执行的操作，用于interp函数，与lab需要完成的code无关
    TransactionState state_;          // 事务状态
    IsolationLevel isolation_level_;  // 事务的隔离级别，默认隔离级别为可串行化
    std::thread::id thread_id_;       // 当前事务对应的线程id
    lsn_t prev_lsn_;                  // 当前事务执行的最后一条操作对应的lsn
    txn_id_t txn_id_;                 // 事务的ID，唯一标识符
    timestamp_t start_ts_;            // 事务的开始时间戳

    std::shared_ptr<std::deque<WriteRecord *>> write_set_;  // 事务包含的所有写操作

    std::shared_ptr<std::unordered_set<LockDataId>> lock_set_;  // 事务申请的所有锁

    /** 用于索引lab: The pages that were latched during index operation, used for concurrent index */
    std::shared_ptr<std::deque<Page *>> page_set_;
    /** 用于索引lab: Concurrent index: the page IDs that were deleted during index operation.*/
    std::shared_ptr<std::deque<Page *>> deleted_page_set_;
};