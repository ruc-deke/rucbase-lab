// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "planner.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "analyze/analyzed_query.h"
#include "optimizer/plan.h"
#include "system/sm_manager.h"

CompOp Planner::reverse_comparison(const CompOp op) {
    switch (op) {
        case OP_EQ:
            return OP_EQ;
        case OP_NE:
            return OP_NE;
        case OP_LT:
            return OP_GT;
        case OP_GT:
            return OP_LT;
        case OP_LE:
            return OP_GE;
        case OP_GE:
            return OP_LE;
    }
    throw InternalError("Unexpected comparison operator");
}

bool Planner::choose_index(const std::string& table,
                           const std::vector<Condition>& conds,
                           std::vector<std::string>& index_columns) const {
    index_columns.clear();
    const TabMeta& tab = sm_manager_->db_.get_table(table);

    const auto has_equality_condition = [&](const ColMeta& column) {
        return std::ranges::any_of(conds, [&](const Condition& condition) {
            return condition.is_rhs_val && condition.op == OP_EQ && condition.lhs_col.tab_name == table &&
                   condition.lhs_col.col_name == column.name;
        });
    };

    for (const auto& index : tab.indexes) {
        const bool matched = std::ranges::all_of(index.cols, has_equality_condition);
        if (!matched || index.cols.size() <= index_columns.size()) {
            continue;
        }

        index_columns.clear();
        index_columns.reserve(index.cols.size());
        for (const auto& column : index.cols) {
            index_columns.push_back(column.name);
        }
    }
    return !index_columns.empty();
}

std::unique_ptr<Plan> Planner::make_scan_plan(const std::string& table, std::vector<Condition> conds) const {
    std::vector<std::string> index_columns;
    const bool use_index = choose_index(table, conds, index_columns);
    const PlanTag scan_tag = use_index ? T_IndexScan : T_SeqScan;
    return std::make_unique<ScanPlan>(scan_tag, sm_manager_, table, std::move(conds), std::move(index_columns));
}

std::vector<Condition> Planner::take_table_conditions(std::vector<Condition>& conditions, const std::string& table) {
    std::vector<Condition> table_conditions;
    auto it = conditions.begin();
    while (it != conditions.end()) {
        const bool value_condition = it->is_rhs_val && it->lhs_col.tab_name == table;
        const bool same_table_cond = !it->is_rhs_val && it->lhs_col.tab_name == table && it->rhs_col.tab_name == table;
        if (value_condition || same_table_cond) {
            table_conditions.emplace_back(std::move(*it));
            it = conditions.erase(it);
        } else {
            ++it;
        }
    }
    return table_conditions;
}

std::size_t Planner::table_index(const std::vector<std::string>& tables, const std::string& table) {
    const auto it = std::ranges::find(tables, table);
    if (it == tables.end()) {
        throw InternalError("Join condition references an unknown table");
    }
    return static_cast<std::size_t>(it - tables.begin());
}

std::unique_ptr<Plan> Planner::take_scan_plan(std::size_t index,
                                              std::vector<bool>& joined,
                                              std::vector<std::unique_ptr<Plan>>& scan_plans) {
    if (index >= scan_plans.size() || joined[index] || scan_plans[index] == nullptr) {
        throw InternalError("Invalid scan plan state while building a join");
    }
    joined[index] = true;
    return std::move(scan_plans[index]);
}

Planner::ConditionCoverage Planner::attach_join_condition(Condition& condition, Plan& plan) {
    if (const auto* scan = dynamic_cast<ScanPlan*>(&plan)) {
        if (scan->tab_name_ == condition.lhs_col.tab_name) {
            return ConditionCoverage::Lhs;
        }
        if (scan->tab_name_ == condition.rhs_col.tab_name) {
            return ConditionCoverage::Rhs;
        }
        return ConditionCoverage::None;
    }

    auto* join = dynamic_cast<JoinPlan*>(&plan);
    if (join == nullptr) {
        return ConditionCoverage::None;
    }

    const ConditionCoverage left = attach_join_condition(condition, *join->left_);
    if (left == ConditionCoverage::Both) {
        return ConditionCoverage::Both;
    }
    const ConditionCoverage right = attach_join_condition(condition, *join->right_);
    if (right == ConditionCoverage::Both) {
        return ConditionCoverage::Both;
    }
    if (left == ConditionCoverage::None) {
        return right;
    }
    if (right == ConditionCoverage::None) {
        return left;
    }
    if (left == ConditionCoverage::Rhs) {
        std::swap(condition.lhs_col, condition.rhs_col);
        condition.op = reverse_comparison(condition.op);
    }
    join->conds_.emplace_back(std::move(condition));
    return ConditionCoverage::Both;
}

std::shared_ptr<AnalyzedQuery> Planner::logical_optimization(std::shared_ptr<AnalyzedQuery> query, Context* context) {
    static_cast<void>(context);
    // TODO 实现逻辑优化规则
    return query;
}

std::unique_ptr<Plan> Planner::physical_optimization(const AnalyzedQuery& query, Context* context) {
    static_cast<void>(context);

    auto root = build_from_plan(query);
    if (query.bound_order_col.has_value()) {
        root = std::make_unique<SortPlan>(T_Sort, std::move(root), *query.bound_order_col, query.bound_order_desc);
    }
    return root;
}

std::unique_ptr<SelectPlan> Planner::plan_select(std::shared_ptr<AnalyzedQuery> query, Context* context) {
    query = logical_optimization(std::move(query), context);
    auto root = physical_optimization(*query, context);
    auto projection =
        std::make_unique<ProjectionPlan>(T_Projection, std::move(root), std::vector<TabCol>(query->bound_cols));
    return std::make_unique<SelectPlan>(std::move(projection));
}

std::unique_ptr<Plan> Planner::build_from_plan(const AnalyzedQuery& query) {
    if (query.bound_tables.empty()) {
        throw InternalError("SELECT query has no bound tables");
    }
    const auto& tables = query.bound_tables;
    std::vector<Condition> remaining_conditions = query.bound_conds;

    std::vector<std::unique_ptr<Plan>> scan_plans;
    scan_plans.reserve(tables.size());
    for (const auto& table : tables) {
        scan_plans.push_back(make_scan_plan(table, take_table_conditions(remaining_conditions, table)));
    }

    if (tables.size() == 1) {
        return std::move(scan_plans.front());
    }
    return build_join_tree(tables, std::move(scan_plans), std::move(remaining_conditions));
}

std::unique_ptr<Plan> Planner::build_join_tree(const std::vector<std::string>& tables,
                                               std::vector<std::unique_ptr<Plan>> scan_plans,
                                               std::vector<Condition> join_conditions) {
    std::vector<bool> joined(tables.size(), false);
    std::unique_ptr<Plan> join_root;

    if (join_conditions.empty()) {
        join_root = take_scan_plan(0, joined, scan_plans);
    } else {
        Condition first = std::move(join_conditions.front());

        const std::size_t lhs = table_index(tables, first.lhs_col.tab_name);
        const std::size_t rhs = table_index(tables, first.rhs_col.tab_name);
        std::vector<Condition> conditions;
        conditions.push_back(std::move(first));
        join_root = std::make_unique<JoinPlan>(T_NestLoop, take_scan_plan(lhs, joined, scan_plans),
                                               take_scan_plan(rhs, joined, scan_plans), std::move(conditions));
    }

    for (std::size_t i = 1; i < join_conditions.size(); ++i) {
        auto& condition = join_conditions[i];
        const std::size_t lhs = table_index(tables, condition.lhs_col.tab_name);
        const std::size_t rhs = table_index(tables, condition.rhs_col.tab_name);
        const bool lhs_joined = joined[lhs];
        const bool rhs_joined = joined[rhs];

        std::vector<Condition> conditions;
        if (!lhs_joined && !rhs_joined) {
            conditions.push_back(std::move(condition));
            auto component = std::make_unique<JoinPlan>(T_NestLoop, take_scan_plan(lhs, joined, scan_plans),
                                                        take_scan_plan(rhs, joined, scan_plans), std::move(conditions));
            join_root = std::make_unique<JoinPlan>(T_NestLoop, std::move(component), std::move(join_root),
                                                   std::vector<Condition>());
        } else if (lhs_joined != rhs_joined) {
            std::size_t new_table = lhs;
            if (lhs_joined) {
                std::swap(condition.lhs_col, condition.rhs_col);
                condition.op = reverse_comparison(condition.op);
                new_table = rhs;
            }
            conditions.push_back(std::move(condition));
            join_root = std::make_unique<JoinPlan>(T_NestLoop, take_scan_plan(new_table, joined, scan_plans),
                                                   std::move(join_root), std::move(conditions));
        } else if (attach_join_condition(condition, *join_root) != ConditionCoverage::Both) {
            throw InternalError("Failed to attach join condition to plan tree");
        }
    }

    for (size_t i = 0; i < tables.size(); ++i) {
        if (!joined[i]) {
            join_root = std::make_unique<JoinPlan>(T_NestLoop, take_scan_plan(i, joined, scan_plans),
                                                   std::move(join_root), std::vector<Condition>());
        }
    }
    return join_root;
}

// 生成DDL语句和DML语句的查询执行计划
std::unique_ptr<Plan> Planner::do_planner(std::shared_ptr<AnalyzedQuery> query, Context* context) {
    switch (query->bound_statement->kind()) {
        case ast::StatementKind::CreateTable: {
            const auto& statement = static_cast<const ast::CreateTableStmt&>(*query->bound_statement);
            std::vector<ColDef> col_defs;
            col_defs.reserve(statement.fields.size());
            for (const auto& field : statement.fields) {
                col_defs.push_back(ColDef{.name = field.col_name,
                                          .type = interpret_type(field.type_len.type),
                                          .len = field.type_len.declared_len});
            }
            return std::make_unique<DDLPlan>(T_CreateTable, statement.tab_name, std::vector<std::string>(), col_defs);
        }
        case ast::StatementKind::DropTable: {
            const auto& statement = static_cast<const ast::DropTableStmt&>(*query->bound_statement);
            return std::make_unique<DDLPlan>(T_DropTable, statement.tab_name, std::vector<std::string>(),
                                             std::vector<ColDef>());
        }
        case ast::StatementKind::CreateIndex: {
            const auto& statement = static_cast<const ast::CreateIndexStmt&>(*query->bound_statement);
            return std::make_unique<DDLPlan>(T_CreateIndex, statement.tab_name, statement.col_names,
                                             std::vector<ColDef>());
        }
        case ast::StatementKind::DropIndex: {
            const auto& statement = static_cast<const ast::DropIndexStmt&>(*query->bound_statement);
            return std::make_unique<DDLPlan>(T_DropIndex, statement.tab_name, statement.col_names,
                                             std::vector<ColDef>());
        }
        case ast::StatementKind::Insert: {
            if (query->bound_target_table.empty()) {
                throw InternalError("INSERT query has no bound target table");
            }
            return std::make_unique<InsertPlan>(query->bound_target_table, query->bound_values);
        }
        case ast::StatementKind::Delete: {
            if (query->bound_target_table.empty()) {
                throw InternalError("DELETE query has no bound target table");
            }
            auto scan = make_scan_plan(query->bound_target_table, query->bound_conds);
            return std::make_unique<DeletePlan>(std::move(scan), query->bound_target_table, query->bound_conds);
        }
        case ast::StatementKind::Update: {
            if (query->bound_target_table.empty()) {
                throw InternalError("UPDATE query has no bound target table");
            }
            auto scan = make_scan_plan(query->bound_target_table, query->bound_conds);
            return std::make_unique<UpdatePlan>(std::move(scan), query->bound_target_table, query->bound_conds,
                                                query->bound_set_clauses);
        }
        case ast::StatementKind::Select: {
            return plan_select(std::move(query), context);
        }
        case ast::StatementKind::Help:
        case ast::StatementKind::ShowTables:
        case ast::StatementKind::ShowDatabase:
        case ast::StatementKind::TxnBegin:
        case ast::StatementKind::TxnCommit:
        case ast::StatementKind::TxnAbort:
        case ast::StatementKind::TxnRollback:
        case ast::StatementKind::DescTable:
            break;
    }
    throw InternalError("Planner received a non-plannable statement kind");
}
