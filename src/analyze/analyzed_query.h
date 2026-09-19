// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "common/common.h"
#include "parser/ast.h"

/**
 * @brief Analyzer 产生的绑定后查询中间结果。
 *
 * bound_statement 保留只读原始 AST，用于语句分派和读取 DDL 载荷；
 * DML 的目标表、列、条件和值均使用下方已绑定字段。
 */
struct AnalyzedQuery {
    explicit AnalyzedQuery(std::shared_ptr<const ast::Statement> statement) : bound_statement(std::move(statement)) {}

    // 只读原始 AST，用于语句分派和 DDL 载荷。
    const std::shared_ptr<const ast::Statement> bound_statement;
    // INSERT、UPDATE 或 DELETE 的目标表。
    std::string bound_target_table;
    // 已完成列绑定和类型检查的 WHERE 条件。
    std::vector<Condition> bound_conds;
    // 已绑定表名的 SELECT 投影列。
    std::vector<TabCol> bound_cols;
    // SELECT 的 FROM 表，保持源码中的从左到右顺序。
    std::vector<std::string> bound_tables;
    // 已绑定的 ORDER BY 列；空表示无排序。
    std::optional<TabCol> bound_order_col;
    // ORDER BY 是否按降序排列。
    bool bound_order_desc = false;
    // UPDATE 的 SET 子句，目标列和右值均已绑定。
    std::vector<SetClause> bound_set_clauses;
    // INSERT 的值列表，已按目标列长度编码。
    std::vector<Value> bound_values;
    // SET 语句：已校验的设置项名称（小写）和布尔取值。
    std::string bound_knob_name;
    bool bound_knob_value = false;
};
