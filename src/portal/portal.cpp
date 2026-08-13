// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "portal/portal.h"

#include <memory>
#include <utility>
#include <vector>

#include "common/errors.h"
#include "execution/execution_manager.h"
#include "execution/execution_sort.h"
#include "execution/executor_abstract.h"
#include "execution/executor_delete.h"
#include "execution/executor_index_scan.h"
#include "execution/executor_insert.h"
#include "execution/executor_nestedloop_join.h"
#include "execution/executor_projection.h"
#include "execution/executor_seq_scan.h"
#include "execution/executor_update.h"
#include "optimizer/plan.h"

namespace {

std::vector<Rid> collect_rids(AbstractExecutor* scan) {
    std::vector<Rid> rids;
    for (scan->begin_tuple(); !scan->is_end(); scan->next_tuple()) {
        rids.push_back(scan->rid());
    }
    return rids;
}

}  // namespace

PortalStmt::PortalStmt(PortalType type_,
                       std::vector<TabCol> sel_cols_,
                       std::unique_ptr<AbstractExecutor> root_,
                       std::unique_ptr<Plan> plan_)
    : type(type_),
      sel_cols(std::move(sel_cols_)),
      root(std::move(root_)),
      plan(std::move(plan_)) {}

PortalStmt::~PortalStmt() = default;

std::unique_ptr<PortalStmt> Portal::start(std::unique_ptr<Plan> plan, Context* context) {
    if (dynamic_cast<OtherPlan*>(plan.get()) != nullptr) {
        return std::make_unique<PortalStmt>(PortalType::Utility, std::vector<TabCol>{}, nullptr, std::move(plan));
    }
    if (dynamic_cast<DDLPlan*>(plan.get()) != nullptr) {
        return std::make_unique<PortalStmt>(PortalType::Ddl, std::vector<TabCol>{}, nullptr, std::move(plan));
    }

    if (auto* select = dynamic_cast<SelectPlan*>(plan.get())) {
        auto root = convert_plan_executor(*select->projection_, context);
        auto selected_columns = std::move(select->projection_->sel_cols_);
        return std::make_unique<PortalStmt>(PortalType::Select, std::move(selected_columns), std::move(root),
                                            std::move(plan));
    }
    if (auto* update = dynamic_cast<UpdatePlan*>(plan.get())) {
        auto scan = convert_plan_executor(*update->scan_, context);
        auto root = std::make_unique<UpdateExecutor>(sm_manager_, update->table_, update->set_clauses_, update->conds_,
                                                     collect_rids(scan.get()), context);
        return std::make_unique<PortalStmt>(PortalType::Dml, std::vector<TabCol>{}, std::move(root), std::move(plan));
    }
    if (auto* delete_plan = dynamic_cast<DeletePlan*>(plan.get())) {
        auto scan = convert_plan_executor(*delete_plan->scan_, context);
        auto root = std::make_unique<DeleteExecutor>(sm_manager_, delete_plan->table_, delete_plan->conds_,
                                                     collect_rids(scan.get()), context);
        return std::make_unique<PortalStmt>(PortalType::Dml, std::vector<TabCol>{}, std::move(root), std::move(plan));
    }
    if (auto* insert = dynamic_cast<InsertPlan*>(plan.get())) {
        auto root = std::make_unique<InsertExecutor>(sm_manager_, insert->table_, insert->values_, context);
        return std::make_unique<PortalStmt>(PortalType::Dml, std::vector<TabCol>{}, std::move(root), std::move(plan));
    }
    throw InternalError("unexpected plan type");
}

void Portal::run(std::unique_ptr<PortalStmt> portal, QlManager* ql, txn_id_t* txn_id, Context* context) {
    switch (portal->type) {
        case PortalType::Select:
            ql->select_from(std::move(portal->root), portal->sel_cols, context);
            return;
        case PortalType::Dml:
            ql->run_dml(std::move(portal->root));
            return;
        case PortalType::Ddl:
            ql->run_mutli_query(*portal->plan, context);
            return;
        case PortalType::Utility:
            ql->run_cmd_utility(*portal->plan, txn_id, context);
            return;
    }
    throw InternalError("unexpected portal type");
}

std::unique_ptr<AbstractExecutor> Portal::convert_plan_executor(Plan& plan, Context* context) {
    if (auto* projection = dynamic_cast<ProjectionPlan*>(&plan)) {
        return std::make_unique<ProjectionExecutor>(convert_plan_executor(*projection->subplan_, context),
                                                    projection->sel_cols_);
    }
    if (auto* scan = dynamic_cast<ScanPlan*>(&plan)) {
        if (scan->tag == T_SeqScan) {
            return std::make_unique<SeqScanExecutor>(sm_manager_, scan->tab_name_, scan->conds_, context);
        }
        return std::make_unique<IndexScanExecutor>(sm_manager_, scan->tab_name_, scan->conds_, scan->index_col_names_,
                                                   context);
    }
    if (auto* join = dynamic_cast<JoinPlan*>(&plan)) {
        return std::make_unique<NestedLoopJoinExecutor>(convert_plan_executor(*join->left_, context),
                                                        convert_plan_executor(*join->right_, context),
                                                        std::move(join->conds_));
    }
    if (auto* sort = dynamic_cast<SortPlan*>(&plan)) {
        return std::make_unique<SortExecutor>(convert_plan_executor(*sort->subplan_, context), sort->sel_col_,
                                              sort->is_desc_);
    }
    throw NotImplementedError("executor conversion for this query plan");
}
