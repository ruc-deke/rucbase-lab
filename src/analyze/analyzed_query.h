// Copyright (c) 2023-2026 Renmin University of China
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

    const std::shared_ptr<const ast::Statement> bound_statement;  // 只读原始 AST，用于语句分派和 DDL 载荷。
    std::string bound_target_table;                               // INSERT、UPDATE 或 DELETE 的目标表。
    std::vector<Condition> bound_conds;                           // 已完成列绑定和类型检查的 WHERE 条件。
    std::vector<TabCol> bound_cols;                               // 已绑定表名的 SELECT 投影列。
    std::vector<std::string> bound_tables;                        // SELECT 的 FROM 表，保持源码中的从左到右顺序。
    std::optional<TabCol> bound_order_col;                        // 已绑定的 ORDER BY 列；空表示无排序。
    bool bound_order_desc = false;                                // ORDER BY 是否按降序排列。
    std::vector<SetClause> bound_set_clauses;                     // UPDATE 的 SET 子句，目标列和右值均已绑定。
    std::vector<Value> bound_values;                              // INSERT 的值列表，已按目标列长度编码。
};
