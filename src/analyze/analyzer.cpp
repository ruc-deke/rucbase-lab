// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "analyze/analyzer.h"

#include <algorithm>
#include <map>
#include <utility>

#include "system/sm_manager.h"

/**
 * @brief 按 AST 根节点分派语义分析，并保留只读原始语句供下游识别类型。
 */
std::shared_ptr<AnalyzedQuery> Analyzer::analyze(const std::shared_ptr<ast::Statement>& statement) {
    auto query = std::make_shared<AnalyzedQuery>();
    if (const auto select = std::dynamic_pointer_cast<ast::SelectStmt>(statement)) {
        analyze_select(*select, *query);
    } else if (const auto update = std::dynamic_pointer_cast<ast::UpdateStmt>(statement)) {
        analyze_update(*update, *query);
    } else if (const auto delete_statement = std::dynamic_pointer_cast<ast::DeleteStmt>(statement)) {
        analyze_delete(*delete_statement, *query);
    } else if (const auto insert = std::dynamic_pointer_cast<ast::InsertStmt>(statement)) {
        analyze_insert(*insert, *query);
    }
    query->bound_statement = statement;
    return query;
}

/**
 * @brief 在 FROM 可见范围内绑定投影列、ORDER BY 和 WHERE。
 *
 * 同一份 all_cols 在三个绑定步骤间复用，避免重复查询目录。
 */
void Analyzer::analyze_select(const ast::SelectStmt& statement, AnalyzedQuery& query) const {
    query.bound_tables = statement.tabs;
    const auto all_cols = get_all_cols(query.bound_tables);

    query.bound_cols.reserve(statement.cols.size());
    for (const auto& ast_col : statement.cols) {
        query.bound_cols.push_back({.tab_name = ast_col->tab_name, .col_name = ast_col->col_name});
    }
    if (query.bound_cols.empty()) {
        query.bound_cols.reserve(all_cols.size());
        for (const auto& col : all_cols) {
            query.bound_cols.push_back({.tab_name = col.tab_name, .col_name = col.name});
        }
    } else {
        for (auto& col : query.bound_cols) {
            col = check_column(all_cols, std::move(col));
        }
    }

    if (statement.order != nullptr) {
        TabCol order_col{.tab_name = statement.order->column->tab_name, .col_name = statement.order->column->col_name};
        query.bound_order_col = check_column(all_cols, std::move(order_col));
        query.bound_order_desc = statement.order->direction == ast::OrderBy_DESC;
    }

    get_clause(statement.conds, query.bound_conds);
    check_clause(all_cols, query.bound_conds);
}

void Analyzer::analyze_update(const ast::UpdateStmt& statement, AnalyzedQuery& query) const {
    const TabMeta& table = sm_manager_->db_.get_table(statement.tab_name);
    query.bound_set_clauses.reserve(statement.set_clauses.size());
    for (const auto& ast_set_clause : statement.set_clauses) {
        SetClause set_clause{
            .lhs = {.tab_name = statement.tab_name, .col_name = ast_set_clause->col_name},
            .rhs = convert_ast_value(ast_set_clause->val),
        };
        const auto column = table.get_col(set_clause.lhs.col_name);
        if (column->type != set_clause.rhs.type) {
            throw IncompatibleTypeError(coltype2str(column->type), coltype2str(set_clause.rhs.type));
        }
        set_clause.rhs.init_raw(column->len);
        query.bound_set_clauses.emplace_back(std::move(set_clause));
    }

    get_clause(statement.conds, query.bound_conds);
    check_clause(table.cols, query.bound_conds);
}

void Analyzer::analyze_delete(const ast::DeleteStmt& statement, AnalyzedQuery& query) const {
    const TabMeta& table = sm_manager_->db_.get_table(statement.tab_name);
    get_clause(statement.conds, query.bound_conds);
    check_clause(table.cols, query.bound_conds);
}

/** @brief 按表定义的列顺序检查 INSERT 值，并预先编码定长存储形式。 */
void Analyzer::analyze_insert(const ast::InsertStmt& statement, AnalyzedQuery& query) const {
    const TabMeta& table = sm_manager_->db_.get_table(statement.tab_name);
    if (statement.vals.size() != table.cols.size()) {
        throw InvalidValueCountError();
    }

    query.bound_values.reserve(statement.vals.size());
    for (size_t i = 0; i < statement.vals.size(); ++i) {
        Value value = convert_ast_value(statement.vals[i]);
        const ColMeta& column = table.cols[i];
        if (value.type != column.type) {
            throw IncompatibleTypeError(coltype2str(column.type), coltype2str(value.type));
        }
        value.init_raw(column.len);
        query.bound_values.emplace_back(std::move(value));
    }
}

/**
 * @brief 验证已限定列，或在给定可见列中解析未限定列。
 * @throws AmbiguousColumnError 未限定列名在多张表中匹配。
 * @throws ColumnNotFoundError 列不存在或不在当前语句的可见范围内。
 */
TabCol Analyzer::check_column(const std::vector<ColMeta>& all_cols, TabCol target) {
    if (target.tab_name.empty()) {
        std::string tab_name;
        for (const auto& col : all_cols) {
            if (col.name == target.col_name) {
                if (!tab_name.empty()) {
                    throw AmbiguousColumnError(target.col_name);
                }
                tab_name = col.tab_name;
            }
        }
        if (tab_name.empty()) {
            throw ColumnNotFoundError(target.col_name);
        }
        target.tab_name = std::move(tab_name);
    } else {
        const auto col = std::ranges::find_if(all_cols, [&](const ColMeta& candidate) {
            return candidate.tab_name == target.tab_name && candidate.name == target.col_name;
        });
        if (col == all_cols.end()) {
            throw ColumnNotFoundError(target.tab_name + '.' + target.col_name);
        }
    }
    return target;
}

/** @brief 按 FROM 顺序收集列元数据；查表过程同时验证表是否存在。 */
std::vector<ColMeta> Analyzer::get_all_cols(const std::vector<std::string>& tab_names) const {
    std::vector<ColMeta> all_cols;
    for (const auto& tab_name : tab_names) {
        const auto& cols = sm_manager_->db_.get_table(tab_name).cols;
        all_cols.insert(all_cols.end(), cols.begin(), cols.end());
    }
    return all_cols;
}

/** @brief 仅将 WHERE AST 转换为 Condition 形状，列绑定和类型检查由 check_clause 完成。 */
void Analyzer::get_clause(const std::vector<std::shared_ptr<ast::BinaryExpr>>& ast_conds,
                          std::vector<Condition>& conds) {
    conds.clear();
    conds.reserve(ast_conds.size());
    for (const auto& expr : ast_conds) {
        Condition cond{};
        cond.lhs_col = {.tab_name = expr->lhs->tab_name, .col_name = expr->lhs->col_name};
        cond.op = convert_ast_comp_op(expr->op);
        if (const auto rhs_value = std::dynamic_pointer_cast<ast::Value>(expr->rhs)) {
            cond.is_rhs_val = true;
            cond.rhs_val = convert_ast_value(rhs_value);
        } else if (const auto rhs_col = std::dynamic_pointer_cast<ast::Col>(expr->rhs)) {
            cond.is_rhs_val = false;
            cond.rhs_col = {.tab_name = rhs_col->tab_name, .col_name = rhs_col->col_name};
        } else {
            throw InternalError("Unexpected condition right-hand side");
        }
        conds.emplace_back(std::move(cond));
    }
}

/**
 * @brief 先将条件两侧列绑定到可见表，再检查类型并编码字面量。
 * @pre all_cols 必须完整覆盖当前语句可见的表。
 */
void Analyzer::check_clause(const std::vector<ColMeta>& all_cols, std::vector<Condition>& conds) const {
    for (auto& cond : conds) {
        cond.lhs_col = check_column(all_cols, std::move(cond.lhs_col));
        if (!cond.is_rhs_val) {
            cond.rhs_col = check_column(all_cols, std::move(cond.rhs_col));
        }

        const auto lhs_col = sm_manager_->db_.get_table(cond.lhs_col.tab_name).get_col(cond.lhs_col.col_name);
        ColType rhs_type;
        if (cond.is_rhs_val) {
            rhs_type = cond.rhs_val.type;
        } else {
            rhs_type = sm_manager_->db_.get_table(cond.rhs_col.tab_name).get_col(cond.rhs_col.col_name)->type;
        }
        if (lhs_col->type != rhs_type) {
            throw IncompatibleTypeError(coltype2str(lhs_col->type), coltype2str(rhs_type));
        }
        if (cond.is_rhs_val) {
            cond.rhs_val.init_raw(lhs_col->len);
        }
    }
}

Value Analyzer::convert_ast_value(const std::shared_ptr<ast::Value>& ast_value) {
    Value value;
    if (const auto integer = std::dynamic_pointer_cast<ast::IntLit>(ast_value)) {
        value.set_int(integer->val);
    } else if (const auto floating_point = std::dynamic_pointer_cast<ast::FloatLit>(ast_value)) {
        value.set_float(floating_point->val);
    } else if (const auto string = std::dynamic_pointer_cast<ast::StringLit>(ast_value)) {
        value.set_str(string->val);
    } else {
        throw InternalError("Unexpected AST value type");
    }
    return value;
}

CompOp Analyzer::convert_ast_comp_op(const ast::SvCompOp op) {
    const std::map<ast::SvCompOp, CompOp> operators = {
        {ast::SV_OP_EQ, OP_EQ}, {ast::SV_OP_NE, OP_NE}, {ast::SV_OP_LT, OP_LT},
        {ast::SV_OP_GT, OP_GT}, {ast::SV_OP_LE, OP_LE}, {ast::SV_OP_GE, OP_GE},
    };
    return operators.at(op);
}
