// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <memory>
#include <vector>

#include "common/common.h"
#include "common/config.h"

class AbstractExecutor;
class Context;
class Plan;
class Planner;
class SmManager;
class TransactionManager;

class QlManager {
private:
    SmManager* sm_manager_;
    TransactionManager* txn_mgr_;
    Planner* planner_;  ///< 保存 SET 语句修改的查询设置；非拥有指针。

public:
    QlManager(SmManager* sm_manager, TransactionManager* txn_mgr, Planner* planner)
        : sm_manager_(sm_manager),
          txn_mgr_(txn_mgr),
          planner_(planner) {}

    // PortalStmt 保留计划所有权；执行管理器只在调用期间借用计划。
    void run_mutli_query(const Plan& plan, Context* context) const;
    void run_cmd_utility(const Plan& plan, txn_id_t* txn_id, Context* context);
    static void select_from(std::unique_ptr<AbstractExecutor> executor_tree_root,
                            const std::vector<TabCol>& sel_cols,
                            Context* context);

    // unique_ptr 按值传入：调用方 std::move 后所有权转移到本函数。
    static void run_dml(std::unique_ptr<AbstractExecutor> exec);
};
