// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "execution_defs.h"
#include "record/rm.h"
#include "system/sm.h"
#include "common/context.h"
#include "common/common.h"
#include "optimizer/plan.h"
#include "executor_abstract.h"
#include "transaction/transaction_manager.h"


class QlManager {
   private:
    SmManager *sm_manager_;
    TransactionManager *txn_mgr_;

   public:
    QlManager(SmManager *sm_manager, TransactionManager *txn_mgr) 
        : sm_manager_(sm_manager),  txn_mgr_(txn_mgr) {}

    // plan / sel_cols 仅只读使用，故用 const 引用，避免 shared_ptr / vector 按值拷贝。
    // 若函数需要接管所有权，应改为按值参数并在内部 std::move（见 Portal::start）。
    void run_mutli_query(const std::shared_ptr<Plan> &plan, Context *context) const;
    void run_cmd_utility(const std::shared_ptr<Plan> &plan, txn_id_t *txn_id, Context *context);
    static void select_from(std::unique_ptr<AbstractExecutor> executorTreeRoot, const std::vector<TabCol> &sel_cols,
                        Context *context);

    // unique_ptr 按值传入：调用方 std::move 后所有权转移到本函数。
    static void run_dml(std::unique_ptr<AbstractExecutor> exec);
};
