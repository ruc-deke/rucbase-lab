/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "transaction_manager.h"
#include "record/rm_file_handle.h"
#include "system/sm_manager.h"

std::unordered_map<txn_id_t, Transaction *> TransactionManager::txn_map = {};

/**
 * @description: 事务的开始方法
 * @return {Transaction*} 开始事务的指针
 * @param {Transaction*} txn 事务指针，空指针代表需要创建新事务，否则开始已有事务
 * @param {LogManager*} log_manager 日志管理器指针
 */
Transaction * TransactionManager::begin(Transaction* txn, LogManager* log_manager) {
    // Todo:
    // 0. 给txn_map_上锁
    std::scoped_lock lock{latch_};
    // 1. 判断传入事务参数是否为空指针
    // 2. 如果为空指针，创建新事务
    if (!txn) {
        txn = new Transaction(next_txn_id_);
        next_txn_id_++;
        // TODO
        // txn->set_txn_mode();
        txn->set_start_ts(next_timestamp_);
    }
    
    // 3. 把开始事务加入到全局事务表中
    txn_map[txn->get_transaction_id()] = txn;
    
    // 4. 返回当前事务指针
    return txn;
}

/**
 * @description: 事务的提交方法
 * @param {Transaction*} txn 需要提交的事务
 * @param {LogManager*} log_manager 日志管理器指针
 */
void TransactionManager::commit(Transaction* txn, LogManager* log_manager) {
    // Todo:
    // 0. 给txn_map_上锁
    std::scoped_lock lock{latch_};
    if (!txn) {
        return;
    }
    // 1. 如果存在未提交的写操作，提交所有的写操作
    auto write_set = txn->get_write_set();
    while (!write_set->empty()) {
        // TODO
        write_set->pop_front();
    }
    // 2. 释放所有锁
    auto lock_set = txn->get_lock_set();
    if (!lock_set->empty()) {
        for (auto it = lock_set->begin(); it != lock_set->end(); it++) {
            lock_manager_->unlock(txn, *it);
        }
    }
    lock_set->clear();
    // 3. 释放事务相关资源， eg.锁集
    // TODO
    // 4. 把事务日志刷入磁盘中
    log_manager->flush_log_to_disk();
    // 5. 更新事务状态
    txn->set_state(TransactionState::COMMITTED);
}

/**
 * @description: 事务的终止（回滚）方法
 * @param {Transaction *} txn 需要回滚的事务
 * @param {LogManager} *log_manager 日志管理器指针
 */
void TransactionManager::abort(Transaction * txn, LogManager *log_manager) {
    // Todo:
    // 0. 给txn_map_上锁
    std::scoped_lock lock{latch_};
    if (!txn) {
        return;
    }
    // 1. 回滚所有写操作
    auto write_set = txn->get_write_set();
    auto* context = new Context(lock_manager_, log_manager, txn);
    for (auto iter = write_set->rbegin(); iter != write_set->rend(); ++iter) {
        auto& type = (*iter)->GetWriteType();
        auto& rid = (*iter)->GetRid();
        auto buf = (*iter)->GetRecord().data;
        auto fh = sm_manager_->fhs_.at((*iter)->GetTableName()).get();
        switch (type) {
            case WType::INSERT_TUPLE:
            fh->delete_record(rid, context);
            break;
        case WType::DELETE_TUPLE:
            fh->insert_record(buf, context);
            break;
        case WType::UPDATE_TUPLE:
            fh->update_record(rid, buf, context);
            break;
        }
    }
    write_set->clear();
    // 2. 释放所有锁
    auto lock_set = txn->get_lock_set();
    if (!lock_set->empty()) {
        for (auto it = lock_set->begin(); it != lock_set->end(); it++) {
            lock_manager_->unlock(txn, *it);
        }
    }
    lock_set->clear();
    // 3. 清空事务相关资源，eg.锁集
    // 4. 把事务日志刷入磁盘中
    log_manager->flush_log_to_disk();
    // 5. 更新事务状态
    txn->set_state(TransactionState::ABORTED);
}
