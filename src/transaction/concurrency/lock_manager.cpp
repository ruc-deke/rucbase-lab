/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "lock_manager.h"

/**
 * @description: 申请行级共享锁
 * @return {bool} 加锁是否成功
 * @param {Transaction*} txn 要申请锁的事务对象指针
 * @param {Rid&} rid 加锁的目标记录ID 记录所在的表的fd
 * @param {int} tab_fd
 */
bool LockManager::lock_shared_on_record(Transaction* txn, const Rid& rid, int tab_fd) {
    
    return true;
}

/**
 * @description: 申请行级排他锁
 * @return {bool} 加锁是否成功
 * @param {Transaction*} txn 要申请锁的事务对象指针
 * @param {Rid&} rid 加锁的目标记录ID
 * @param {int} tab_fd 记录所在的表的fd
 */
bool LockManager::lock_exclusive_on_record(Transaction* txn, const Rid& rid, int tab_fd) {
    // TODO 2.:
    // 0. 对锁表加latch
    std::unique_lock<std::mutex> lock{latch_};
    // 1. 检查并更新事务状态是否遵循两阶段封锁原则
    TransactionState txn_stat = txn->get_state();
    if (txn_stat == TransactionState::SHRINKING) {
        throw TransactionAbortException(txn->get_transaction_id(), AbortReason::LOCK_ON_SHIRINKING);
        return false;
    } else if (txn_stat == TransactionState::ABORTED || txn_stat == TransactionState::COMMITTED) {
    // TODO how to handle this
        return false;
    }
    txn->set_state(TransactionState::GROWING);
    // 2. 检查锁表中的锁是否与当前申请的写锁冲突
    LockDataId rec_lockID = LockDataId(tab_fd, rid, LockDataType::RECORD);
    std::list<LockRequest>::iterator lock_it;
    bool lock_found = false;
    for (auto it = lock_table_[rec_lockID].request_queue_.begin(); it != lock_table_[rec_lockID].request_queue_.end(); ++it) {
        if (it->txn_id_ != txn->get_transaction_id()) {
            throw TransactionAbortException(txn->get_transaction_id(), AbortReason::DEADLOCK_PREVENTION);
            return false;
        } else {
            if (it->lock_mode_ == LockMode::EXLUCSIVE) {
                return true; // 已有x
            } else {
                lock_it = it;
                lock_found = true;
            }
        }
    }
    auto txn_locks = txn->get_lock_set();
    // if (txn_locks->find(rec_lockID) != txn_locks->end()) {
    //     return true;
    // }
    // 3. 检查本事务是否已经持有当前记录的锁，有读锁则升级成写锁
    if (lock_found) {
        lock_it->lock_mode_ = LockMode::EXLUCSIVE;
        if (lock_table_[rec_lockID].group_lock_mode_ < GroupLockMode::X) {
            lock_table_[rec_lockID].group_lock_mode_ = GroupLockMode::X;
        }
        return true;
    }
    // 4. 没有其它锁，也没有自身的锁，颁发新的锁
    txn_locks->insert(rec_lockID);
    LockRequest lock_request = LockRequest(txn->get_transaction_id(), LockMode::EXLUCSIVE);
    lock_request.granted_ = true;
    lock_table_[rec_lockID].request_queue_.push_back(lock_request);
    if (lock_table_[rec_lockID].group_lock_mode_ < GroupLockMode::X) {
        lock_table_[rec_lockID].group_lock_mode_ = GroupLockMode::X;
    }
    return true;
}

/**
 * @description: 申请表级读锁
 * @return {bool} 返回加锁是否成功
 * @param {Transaction*} txn 要申请锁的事务对象指针
 * @param {int} tab_fd 目标表的fd
 */
bool LockManager::lock_shared_on_table(Transaction* txn, int tab_fd) {
    
    return true;
}

/**
 * @description: 申请表级写锁
 * @return {bool} 返回加锁是否成功
 * @param {Transaction*} txn 要申请锁的事务对象指针
 * @param {int} tab_fd 目标表的fd
 */
bool LockManager::lock_exclusive_on_table(Transaction* txn, int tab_fd) {
    
    return true;
}

/**
 * @description: 申请表级意向读锁
 * @return {bool} 返回加锁是否成功
 * @param {Transaction*} txn 要申请锁的事务对象指针
 * @param {int} tab_fd 目标表的fd
 */
bool LockManager::lock_IS_on_table(Transaction* txn, int tab_fd) {
    
    return true;
}

/**
 * @description: 申请表级意向写锁
 * @return {bool} 返回加锁是否成功
 * @param {Transaction*} txn 要申请锁的事务对象指针
 * @param {int} tab_fd 目标表的fd
 */
bool LockManager::lock_IX_on_table(Transaction* txn, int tab_fd) {
    
    return true;
}

/**
 * @description: 释放锁
 * @return {bool} 返回解锁是否成功
 * @param {Transaction*} txn 要释放锁的事务对象指针
 * @param {LockDataId} lock_data_id 要释放的锁ID
 */
bool LockManager::unlock(Transaction* txn, LockDataId lock_data_id) {
   
    return true;
}
