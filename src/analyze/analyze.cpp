// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "analyze.h"

#include <algorithm>

/**
 * @description: 分析器，进行语义分析和查询重写，需要检查不符合语义规定的部分
 * @param {shared_ptr<ast::TreeNode>} parse parser生成的结果集
 * @return {shared_ptr<Query>} Query
 */
std::shared_ptr<Query> Analyze::do_analyze(std::shared_ptr<ast::Statement> parse) {
    std::shared_ptr<Query> query = std::make_shared<Query>();
    if (const auto select_stmt = std::dynamic_pointer_cast<ast::SelectStmt>(parse)) {
        // 处理表名
        query->tables = std::move(select_stmt->tabs);
        // 检查表是否存在
        for (const auto& tbl : query->tables) {
            if (!sm_manager_->db_.is_table(tbl)) {
                throw TableNotFoundError(tbl);
            }
        }

        // 处理target list，再target list中添加上表名，例如 a.id
        for (const auto& sv_sel_col : select_stmt->cols) {
            TabCol sel_col = {.tab_name = sv_sel_col->tab_name, .col_name = sv_sel_col->col_name};
            query->cols.push_back(sel_col);
        }
        // auto all_cols = get_all_cols(query->tables);
        std::vector<ColMeta> all_cols;
        get_all_cols(query->tables, all_cols);
        if (query->cols.empty()) {
            // select all columns
            for (const auto& col : all_cols) {
                TabCol sel_col = {.tab_name = col.tab_name, .col_name = col.name};
                query->cols.push_back(sel_col);
            }
        } else {
            // infer table name from column name
            for (auto& sel_col : query->cols) {
                sel_col = check_column(all_cols, sel_col);  // 列元数据校验
            }
        }
        if (select_stmt->order != nullptr) {
            TabCol order_col{.tab_name = select_stmt->order->column->tab_name,
                             .col_name = select_stmt->order->column->col_name};
            query->order_col = check_column(all_cols, std::move(order_col));
            query->order_desc = select_stmt->order->direction == ast::OrderBy_DESC;
        }
        // 处理where条件
        get_clause(select_stmt->conds, query->conds);
        check_clause(query->tables, query->conds);
    } else if (const auto update_stmt = std::dynamic_pointer_cast<ast::UpdateStmt>(parse)) {
        // 处理 update 的set 值
        for (const auto& sv_set_clause : update_stmt->set_clauses) {
            SetClause set_clause = {.lhs = {.tab_name = "", .col_name = sv_set_clause->col_name},
                                    .rhs = convert_sv_value(sv_set_clause->val)};
            query->set_clauses.push_back(set_clause);
        }
        const TabMeta& tab = sm_manager_->db_.get_table(update_stmt->tab_name);
        // C++17 结构化绑定：SetClause{lhs, rhs} 拆成两个引用，可原地修改 rhs。
        for (auto& [lhs, rhs] : query->set_clauses) {
            const auto lhs_col = tab.get_col(lhs.col_name);
            if (lhs_col->type != rhs.type) {
                throw IncompatibleTypeError(coltype2str(lhs_col->type), coltype2str(rhs.type));
            }
            rhs.init_raw(lhs_col->len);
        }
        // 处理where条件
        get_clause(update_stmt->conds, query->conds);
        check_clause({update_stmt->tab_name}, query->conds);
    } else if (const auto delete_stmt = std::dynamic_pointer_cast<ast::DeleteStmt>(parse)) {
        // 处理where条件
        get_clause(delete_stmt->conds, query->conds);
        check_clause({delete_stmt->tab_name}, query->conds);
    } else if (const auto insert_stmt = std::dynamic_pointer_cast<ast::InsertStmt>(parse)) {
        // 处理insert 的values值
        for (const auto& sv_val : insert_stmt->vals) {
            query->values.push_back(convert_sv_value(sv_val));
        }
    } else {
        // do nothing
    }
    query->parse = std::move(parse);
    return query;
}

TabCol Analyze::check_column(const std::vector<ColMeta>& all_cols, TabCol target) {
    if (target.tab_name.empty()) {
        // Table name not specified, infer table name from column name
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
        target.tab_name = tab_name;
    } else {
        // Qualified columns must still belong to one of the tables in this query.
        // C++20 ranges::find_if：在 all_cols 中找匹配 (表名, 列名) 的字段元数据。
        const auto col = std::ranges::find_if(all_cols, [&](const ColMeta& candidate) {
            return candidate.tab_name == target.tab_name && candidate.name == target.col_name;
        });
        if (col == all_cols.end()) {
            throw ColumnNotFoundError(target.tab_name + '.' + target.col_name);
        }
    }
    return target;
}

void Analyze::get_all_cols(const std::vector<std::string>& tab_names, std::vector<ColMeta>& all_cols) const {
    for (const auto& sel_tab_name : tab_names) {
        // 这里db_不能写成get_db(), 注意要传指针
        const auto& sel_tab_cols = sm_manager_->db_.get_table(sel_tab_name).cols;
        all_cols.insert(all_cols.end(), sel_tab_cols.begin(), sel_tab_cols.end());
    }
}

void Analyze::get_clause(const std::vector<std::shared_ptr<ast::BinaryExpr>>& sv_conds, std::vector<Condition>& conds) {
    conds.clear();
    for (auto& expr : sv_conds) {
        Condition cond;
        cond.lhs_col = {.tab_name = expr->lhs->tab_name, .col_name = expr->lhs->col_name};
        cond.op = convert_sv_comp_op(expr->op);
        if (auto rhs_val = std::dynamic_pointer_cast<ast::Value>(expr->rhs)) {
            cond.is_rhs_val = true;
            cond.rhs_val = convert_sv_value(rhs_val);
        } else if (auto rhs_col = std::dynamic_pointer_cast<ast::Col>(expr->rhs)) {
            cond.is_rhs_val = false;
            cond.rhs_col = {.tab_name = rhs_col->tab_name, .col_name = rhs_col->col_name};
        }
        conds.push_back(cond);
    }
}

void Analyze::check_clause(const std::vector<std::string>& tab_names, std::vector<Condition>& conds) {
    // auto all_cols = get_all_cols(tab_names);
    std::vector<ColMeta> all_cols;
    get_all_cols(tab_names, all_cols);
    // Get raw values in where clause
    for (auto& cond : conds) {
        // Infer table name from column name
        cond.lhs_col = check_column(all_cols, cond.lhs_col);
        if (!cond.is_rhs_val) {
            cond.rhs_col = check_column(all_cols, cond.rhs_col);
        }
        const TabMeta& lhs_tab = sm_manager_->db_.get_table(cond.lhs_col.tab_name);
        const auto lhs_col = lhs_tab.get_col(cond.lhs_col.col_name);
        const ColType lhs_type = lhs_col->type;
        ColType rhs_type;
        if (cond.is_rhs_val) {
            rhs_type = cond.rhs_val.type;
        } else {
            const TabMeta& rhs_tab = sm_manager_->db_.get_table(cond.rhs_col.tab_name);
            const auto rhs_col = rhs_tab.get_col(cond.rhs_col.col_name);
            rhs_type = rhs_col->type;
        }
        if (lhs_type != rhs_type) {
            throw IncompatibleTypeError(coltype2str(lhs_type), coltype2str(rhs_type));
        }
        if (cond.is_rhs_val) {
            cond.rhs_val.init_raw(lhs_col->len);
        }
    }
}

Value Analyze::convert_sv_value(const std::shared_ptr<ast::Value>& sv_val) {
    Value val;
    if (const auto int_lit = std::dynamic_pointer_cast<ast::IntLit>(sv_val)) {
        val.set_int(int_lit->val);
    } else if (const auto float_lit = std::dynamic_pointer_cast<ast::FloatLit>(sv_val)) {
        val.set_float(float_lit->val);
    } else if (const auto str_lit = std::dynamic_pointer_cast<ast::StringLit>(sv_val)) {
        val.set_str(str_lit->val);
    } else {
        throw InternalError("Unexpected sv value type");
    }
    return val;
}

CompOp Analyze::convert_sv_comp_op(ast::SvCompOp op) {
    const std::map<ast::SvCompOp, CompOp> m = {
        {ast::SV_OP_EQ, OP_EQ}, {ast::SV_OP_NE, OP_NE}, {ast::SV_OP_LT, OP_LT},
        {ast::SV_OP_GT, OP_GT}, {ast::SV_OP_LE, OP_LE}, {ast::SV_OP_GE, OP_GE},
    };
    return m.at(op);
}
