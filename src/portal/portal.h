// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

/**
 * @file portal.h
 * @brief 定义逻辑执行计划与物理执行器之间的 Portal 接口。
 */

#pragma once

#include <memory>
#include <vector>

#include "common/common.h"
#include "common/config.h"

class AbstractExecutor;
class Context;
class Plan;
class QlManager;
class SmManager;

/** @brief Portal 接收到的语句类别。 */
enum class PortalType {
    Select,
    Dml,
    Ddl,
    Utility,
};

/** @brief Portal 为执行阶段准备的一条语句。 */
struct PortalStmt {
    PortalType type;
    std::vector<TabCol> sel_cols;
    std::unique_ptr<AbstractExecutor> root;
    std::shared_ptr<Plan> plan;

    PortalStmt(PortalType type_,
               std::vector<TabCol> sel_cols_,
               std::unique_ptr<AbstractExecutor> root_,
               std::shared_ptr<Plan> plan_);
    ~PortalStmt();

    PortalStmt(const PortalStmt&) = delete;
    PortalStmt& operator=(const PortalStmt&) = delete;
};

/**
 * @brief 将 Planner 生成的计划转换为执行器树，并交给执行层运行。
 *
 * Portal 不拥有数据库组件；其生命周期由顶层 Server 统一管理。
 */
class Portal {
public:
    explicit Portal(SmManager* sm_manager) : sm_manager_(sm_manager) {}

    std::unique_ptr<PortalStmt> start(std::shared_ptr<Plan> plan, Context* context);
    void run(std::unique_ptr<PortalStmt> portal, QlManager* ql, txn_id_t* txn_id, Context* context);

private:
    std::unique_ptr<AbstractExecutor> convert_plan_executor(std::shared_ptr<Plan> plan, Context* context);

    SmManager* sm_manager_;
};
