// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cassert>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "parser/parser.h"
#include "system/sm.h"
#include "common/common.h"

class Query{
    public:
    std::shared_ptr<ast::Statement> parse;
    // where条件
    std::vector<Condition> conds;
    // 投影列
    std::vector<TabCol> cols;
    // 表名
    std::vector<std::string> tables;
    // ORDER BY 列（已在 Analyze 阶段绑定到 FROM 范围内的表）
    std::optional<TabCol> order_col;
    bool order_desc = false;
    // update 的set 值
    std::vector<SetClause> set_clauses;
    //insert 的values值
    std::vector<Value> values;

    Query() = default;

};

class Analyze
{
private:
    SmManager *sm_manager_;
public:
    explicit Analyze(SmManager *sm_manager) : sm_manager_(sm_manager){}
    ~Analyze() = default;

    std::shared_ptr<Query> do_analyze(std::shared_ptr<ast::Statement> root);

private:
    static TabCol check_column(const std::vector<ColMeta> &all_cols, TabCol target);
    void get_all_cols(const std::vector<std::string> &tab_names, std::vector<ColMeta> &all_cols) const;
    static void get_clause(const std::vector<std::shared_ptr<ast::BinaryExpr>> &sv_conds,
                           std::vector<Condition> &conds);
    void check_clause(const std::vector<std::string> &tab_names, std::vector<Condition> &conds);
    static Value convert_sv_value(const std::shared_ptr<ast::Value> &sv_val);
    static CompOp convert_sv_comp_op(ast::SvCompOp op);
};
