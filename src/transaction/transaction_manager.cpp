// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "transaction_manager.h"

/**
 * @brief 创建新事务并由 *this 独占。
 * @note 不要从外部传入 Transaction*。所有事务都必须由本函数 make_unique 后放入 txn_map_。
 */
Transaction* TransactionManager::begin(LogManager* log_manager) {
    // Todo:
    // 1. 分配新的 txn_id，make_unique<Transaction>。
    // 2. 把 unique_ptr 移入 txn_map_。
    // 3. 返回表中的借用指针。

    return nullptr;
}

/**
 * @brief 事务的提交方法。
 */
void TransactionManager::commit(Transaction* txn, LogManager* log_manager) {
    // Todo:
    // 释放锁，清理写集，将事务从 txn_map_ 移除（unique_ptr 析构即释放对象）。
}

/**
 * @brief 事务的终止（回滚）方法。
 */
void TransactionManager::abort(Transaction* txn, LogManager* log_manager) {
    // Todo:
    // 按 write_set() 中的值记录撤销写操作，释放锁，再从 txn_map_ 移除事务。
}
