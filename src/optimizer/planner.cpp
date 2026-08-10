// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "planner.h"

#include <algorithm>
#include <memory>

#include "analyze/analyzed_query.h"
#include "optimizer/plan.h"
#include "system/sm_manager.h"

// 目前的索引匹配规则为：完全匹配索引字段，且全部为单点查询，不会自动调整where条件的顺序
bool Planner::get_index_cols(const std::string& tab_name,
                             const std::vector<Condition>& curr_conds,
                             std::vector<std::string>& index_col_names) const {
    index_col_names.clear();
    for (const auto& cond : curr_conds) {
        if (cond.is_rhs_val && cond.op == OP_EQ && cond.lhs_col.tab_name.compare(tab_name) == 0)
            index_col_names.push_back(cond.lhs_col.col_name);
    }
    const TabMeta& tab = sm_manager_->db_.get_table(tab_name);
    if (tab.is_index(index_col_names)) return true;
    return false;
}

/**
 * @brief 表算子条件谓词生成
 *
 * @param conds 条件
 * @param tab_name 表名
 * @return std::vector<Condition>
 */
std::vector<Condition> pop_conds(std::vector<Condition>& conds, const std::string& tab_name) {
    // auto has_tab = [&](const std::string &tab_name) {
    //     return std::ranges::find(tab_names, tab_name) != tab_names.end();
    // };
    std::vector<Condition> solved_conds;
    auto it = conds.begin();
    while (it != conds.end()) {
        const bool value_cond = it->is_rhs_val && it->lhs_col.tab_name == tab_name;
        const bool same_table_cond =
            !it->is_rhs_val && it->lhs_col.tab_name == tab_name && it->rhs_col.tab_name == tab_name;
        if (value_cond || same_table_cond) {
            solved_conds.emplace_back(std::move(*it));
            it = conds.erase(it);
        } else {
            it++;
        }
    }
    return solved_conds;
}

// plan 只用于向下递归观察，不转移所有权，故用 const shared_ptr& 避免引用计数拷贝。
int push_conds(Condition* cond, const std::shared_ptr<Plan>& plan) {
    if (const auto scan_plan = std::dynamic_pointer_cast<ScanPlan>(plan)) {
        if (scan_plan->tab_name_.compare(cond->lhs_col.tab_name) == 0) {
            return 1;
        } else if (scan_plan->tab_name_.compare(cond->rhs_col.tab_name) == 0) {
            return 2;
        } else {
            return 0;
        }
    } else if (const auto join_plan = std::dynamic_pointer_cast<JoinPlan>(plan)) {
        const int left_res = push_conds(cond, join_plan->left_);
        // 条件已经下推到左子节点
        if (left_res == 3) {
            return 3;
        }
        const int right_res = push_conds(cond, join_plan->right_);
        // 条件已经下推到右子节点
        if (right_res == 3) {
            return 3;
        }
        // 左子节点或右子节点有一个没有匹配到条件的列
        if (left_res == 0 || right_res == 0) {
            return left_res + right_res;
        }
        // 左子节点匹配到条件的右边
        if (left_res == 2) {
            // 需要将左右两边的条件变换位置
            const std::map<CompOp, CompOp> swap_op = {
                {OP_EQ, OP_EQ}, {OP_NE, OP_NE}, {OP_LT, OP_GT}, {OP_GT, OP_LT}, {OP_LE, OP_GE}, {OP_GE, OP_LE},
            };
            std::swap(cond->lhs_col, cond->rhs_col);
            cond->op = swap_op.at(cond->op);
        }
        join_plan->conds_.emplace_back(std::move(*cond));
        return 3;
    }
    return false;
}

std::shared_ptr<Plan> pop_scan(int* scantbl,
                               const std::string& table,
                               std::vector<std::string>& joined_tables,
                               const std::vector<std::shared_ptr<Plan>>& plans) {
    for (size_t i = 0; i < plans.size(); i++) {
        const auto scan_plan = std::dynamic_pointer_cast<ScanPlan>(plans[i]);
        if (scan_plan->tab_name_.compare(table) == 0) {
            scantbl[i] = 1;
            joined_tables.emplace_back(scan_plan->tab_name_);
            return plans[i];
        }
    }
    return nullptr;
}

std::shared_ptr<AnalyzedQuery> Planner::logical_optimization(std::shared_ptr<AnalyzedQuery> query, Context* context) {
    // TODO 实现逻辑优化规则

    return query;
}

std::shared_ptr<Plan> Planner::physical_optimization(const std::shared_ptr<AnalyzedQuery>& query, Context* context) {
    std::shared_ptr<Plan> plan = make_one_rel(query);

    // 其他物理优化

    // 处理orderby
    plan = generate_sort_plan(query, std::move(plan));

    return plan;
}

std::shared_ptr<Plan> Planner::make_one_rel(const std::shared_ptr<AnalyzedQuery>& query) {
    const std::vector<std::string>& tables = query->bound_tables;
    std::vector<Condition> remaining_conds = query->bound_conds;
    // // Scan table , 生成表算子列表tab_nodes
    std::vector<std::shared_ptr<Plan>> table_scan_executors(tables.size());
    for (size_t i = 0; i < tables.size(); i++) {
        const auto curr_conds = pop_conds(remaining_conds, tables[i]);
        // int index_no = get_indexNo(tables[i], curr_conds);
        std::vector<std::string> index_col_names;
        const bool index_exist = get_index_cols(tables[i], curr_conds, index_col_names);
        if (index_exist == false) {  // 该表没有索引
            index_col_names.clear();
            table_scan_executors[i] =
                std::make_shared<ScanPlan>(T_SeqScan, sm_manager_, tables[i], curr_conds, index_col_names);
        } else {  // 存在索引
            table_scan_executors[i] =
                std::make_shared<ScanPlan>(T_IndexScan, sm_manager_, tables[i], curr_conds, index_col_names);
        }
    }
    // 只有一个表，不需要join。
    if (tables.size() == 1) {
        return table_scan_executors[0];
    }
    // 获取where条件
    auto conds = std::move(remaining_conds);
    std::shared_ptr<Plan> table_join_executors;

    std::vector<int> scantbl(tables.size(), -1);
    if (conds.size() >= 1) {
        // 有连接条件

        // 根据连接条件，生成第一层join
        std::vector<std::string> joined_tables(tables.size());
        auto it = conds.begin();
        while (it != conds.end()) {
            std::shared_ptr<Plan> left, right;
            left = pop_scan(scantbl.data(), it->lhs_col.tab_name, joined_tables, table_scan_executors);
            right = pop_scan(scantbl.data(), it->rhs_col.tab_name, joined_tables, table_scan_executors);
            std::vector<Condition> join_conds{*it};
            // 建立join
            table_join_executors =
                std::make_shared<JoinPlan>(T_NestLoop, std::move(left), std::move(right), join_conds);
            conds.erase(it);
            break;
        }
        // 根据连接条件，生成第2-n层join
        it = conds.begin();
        while (it != conds.end()) {
            std::shared_ptr<Plan> left_need_to_join_executors = nullptr;
            std::shared_ptr<Plan> right_need_to_join_executors = nullptr;
            bool isneedreverse = false;
            // C++20 ranges::find：表是否已加入 join 序列；未加入则弹出对应扫描计划。
            if (std::ranges::find(joined_tables, it->lhs_col.tab_name) == joined_tables.end()) {
                left_need_to_join_executors =
                    pop_scan(scantbl.data(), it->lhs_col.tab_name, joined_tables, table_scan_executors);
            }
            if (std::ranges::find(joined_tables, it->rhs_col.tab_name) == joined_tables.end()) {
                right_need_to_join_executors =
                    pop_scan(scantbl.data(), it->rhs_col.tab_name, joined_tables, table_scan_executors);
                isneedreverse = true;
            }

            if (left_need_to_join_executors != nullptr && right_need_to_join_executors != nullptr) {
                std::vector<Condition> join_conds{*it};
                std::shared_ptr<Plan> temp_join_executors =
                    std::make_shared<JoinPlan>(T_NestLoop, std::move(left_need_to_join_executors),
                                               std::move(right_need_to_join_executors), join_conds);
                table_join_executors =
                    std::make_shared<JoinPlan>(T_NestLoop, std::move(temp_join_executors),
                                               std::move(table_join_executors), std::vector<Condition>());
            } else if (left_need_to_join_executors != nullptr || right_need_to_join_executors != nullptr) {
                if (isneedreverse) {
                    const std::map<CompOp, CompOp> swap_op = {
                        {OP_EQ, OP_EQ}, {OP_NE, OP_NE}, {OP_LT, OP_GT}, {OP_GT, OP_LT}, {OP_LE, OP_GE}, {OP_GE, OP_LE},
                    };
                    std::swap(it->lhs_col, it->rhs_col);
                    it->op = swap_op.at(it->op);
                    left_need_to_join_executors = std::move(right_need_to_join_executors);
                }
                std::vector<Condition> join_conds{*it};
                table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, std::move(left_need_to_join_executors),
                                                                  std::move(table_join_executors), join_conds);
            } else {
                push_conds(&(*it), table_join_executors);
            }
            it = conds.erase(it);
        }
    } else {
        table_join_executors = table_scan_executors[0];
        scantbl[0] = 1;
    }

    // 连接剩余表
    for (size_t i = 0; i < tables.size(); i++) {
        if (scantbl[i] == -1) {
            table_join_executors =
                std::make_shared<JoinPlan>(T_NestLoop, std::move(table_scan_executors[i]),
                                           std::move(table_join_executors), std::vector<Condition>());
        }
    }

    return table_join_executors;
}

std::shared_ptr<Plan> Planner::generate_sort_plan(const std::shared_ptr<AnalyzedQuery>& query,
                                                  std::shared_ptr<Plan> plan) {
    if (!query->bound_order_col.has_value()) {
        return plan;
    }
    return std::make_shared<SortPlan>(T_Sort, std::move(plan), *query->bound_order_col, query->bound_order_desc);
}

/**
 * @brief select plan 生成
 *
 * @param query Analyzer 阶段产生的查询。
 * @param context 当前请求的执行上下文。
 * @return 顶层为投影算子的查询计划。
 */
std::shared_ptr<Plan> Planner::generate_select_plan(std::shared_ptr<AnalyzedQuery> query, Context* context) {
    // 逻辑优化
    query = logical_optimization(std::move(query), context);

    // 物理优化
    auto sel_cols = query->bound_cols;
    std::shared_ptr<Plan> plannerRoot = physical_optimization(query, context);
    plannerRoot = std::make_shared<ProjectionPlan>(T_Projection, std::move(plannerRoot), std::move(sel_cols));

    return plannerRoot;
}

// 生成DDL语句和DML语句的查询执行计划
std::shared_ptr<Plan> Planner::do_planner(std::shared_ptr<AnalyzedQuery> query, Context* context) {
    std::shared_ptr<Plan> plannerRoot;
    if (const auto create_table = std::dynamic_pointer_cast<const ast::CreateTable>(query->bound_statement)) {
        // create table;
        std::vector<ColDef> col_defs;
        col_defs.reserve(create_table->fields.size());
        for (const auto& field : create_table->fields) {
            col_defs.push_back(ColDef{.name = field.col_name,
                                      .type = interp_sv_type(field.type_len.type),
                                      .len = field.type_len.len});
        }
        plannerRoot =
            std::make_shared<DDLPlan>(T_CreateTable, create_table->tab_name, std::vector<std::string>(), col_defs);
    } else if (const auto drop_table = std::dynamic_pointer_cast<const ast::DropTable>(query->bound_statement)) {
        // drop table;
        plannerRoot =
            std::make_shared<DDLPlan>(T_DropTable, drop_table->tab_name, std::vector<std::string>(),
                                      std::vector<ColDef>());
    } else if (const auto create_index = std::dynamic_pointer_cast<const ast::CreateIndex>(query->bound_statement)) {
        // create index;
        plannerRoot = std::make_shared<DDLPlan>(T_CreateIndex, create_index->tab_name, create_index->col_names,
                                                std::vector<ColDef>());
    } else if (const auto drop_index = std::dynamic_pointer_cast<const ast::DropIndex>(query->bound_statement)) {
        // drop index
        plannerRoot = std::make_shared<DDLPlan>(T_DropIndex, drop_index->tab_name, drop_index->col_names,
                                                std::vector<ColDef>());
    } else if (const auto insert_stmt = std::dynamic_pointer_cast<const ast::InsertStmt>(query->bound_statement)) {
        // insert;
        plannerRoot = std::make_shared<DMLPlan>(T_Insert, std::shared_ptr<Plan>(), insert_stmt->tab_name,
                                                query->bound_values, std::vector<Condition>(),
                                                std::vector<SetClause>());
    } else if (const auto delete_stmt =
                   std::dynamic_pointer_cast<const ast::DeleteStmt>(query->bound_statement)) {
        // delete;
        // 生成表扫描方式
        std::shared_ptr<Plan> table_scan_executors;
        // 只有一张表，不需要进行物理优化了
        // int index_no = get_indexNo(x->tab_name, query->bound_conds);
        std::vector<std::string> index_col_names;
        const bool index_exist = get_index_cols(delete_stmt->tab_name, query->bound_conds, index_col_names);

        if (index_exist == false) {  // 该表没有索引
            index_col_names.clear();
            table_scan_executors =
                std::make_shared<ScanPlan>(T_SeqScan, sm_manager_, delete_stmt->tab_name, query->bound_conds,
                                           index_col_names);
        } else {  // 存在索引
            table_scan_executors =
                std::make_shared<ScanPlan>(T_IndexScan, sm_manager_, delete_stmt->tab_name, query->bound_conds,
                                           index_col_names);
        }

        plannerRoot = std::make_shared<DMLPlan>(T_Delete, table_scan_executors, delete_stmt->tab_name,
                                                std::vector<Value>(),
                                                query->bound_conds, std::vector<SetClause>());
    } else if (const auto update_stmt =
                   std::dynamic_pointer_cast<const ast::UpdateStmt>(query->bound_statement)) {
        // update;
        // 生成表扫描方式
        std::shared_ptr<Plan> table_scan_executors;
        // 只有一张表，不需要进行物理优化了
        // int index_no = get_indexNo(x->tab_name, query->bound_conds);
        std::vector<std::string> index_col_names;
        const bool index_exist = get_index_cols(update_stmt->tab_name, query->bound_conds, index_col_names);

        if (index_exist == false) {  // 该表没有索引
            index_col_names.clear();
            table_scan_executors =
                std::make_shared<ScanPlan>(T_SeqScan, sm_manager_, update_stmt->tab_name, query->bound_conds,
                                           index_col_names);
        } else {  // 存在索引
            table_scan_executors =
                std::make_shared<ScanPlan>(T_IndexScan, sm_manager_, update_stmt->tab_name, query->bound_conds,
                                           index_col_names);
        }
        plannerRoot = std::make_shared<DMLPlan>(T_Update, table_scan_executors, update_stmt->tab_name,
                                                std::vector<Value>(),
                                                query->bound_conds, query->bound_set_clauses);
    } else if (std::dynamic_pointer_cast<const ast::SelectStmt>(query->bound_statement)) {
        // 生成select语句的查询执行计划
        std::shared_ptr<Plan> projection = generate_select_plan(std::move(query), context);
        plannerRoot = std::make_shared<DMLPlan>(T_select, projection, std::string(), std::vector<Value>(),
                                                std::vector<Condition>(), std::vector<SetClause>());
    } else {
        throw InternalError("Unexpected AST root");
    }
    return plannerRoot;
}
