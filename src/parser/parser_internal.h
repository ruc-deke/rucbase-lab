// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

/**
 * @file parser_internal.h
 * @brief Flex scanner 与 Bison parser 共享的内部数据。
 * @internal
 *
 * 数据库其他模块必须包含 parser.h，而不是本文件。SemanticValue 包含较多
 * 字段，这是当前 Bison C skeleton 的实现约束，不属于公共 AST 模型。
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "parser/parser.h"

namespace rucbase::parser {

/**
 * @brief 在 Flex token 与 Bison 产生式之间传递的语义值。
 * @internal
 *
 * 每个语法符号只使用 yacc.y 中通过 `%token` 或 `%type` 为其声明的成员。
 */
struct SemanticValue {
    int integer = 0;
    float floating_point = 0;
    std::string text;
    std::vector<std::string> column_names;

    std::shared_ptr<ast::Statement> statement;
    ast::CompOp comparison_operator = ast::CompOp::Eq;
    ast::TypeLen type_length;
    ast::ColDef column_definition;
    std::vector<ast::ColDef> column_definitions;
    std::shared_ptr<ast::Expr> expression;
    std::vector<std::shared_ptr<ast::Expr>> expressions;
    std::shared_ptr<ast::Value> value;
    std::vector<std::shared_ptr<ast::Value>> values;
    std::shared_ptr<ast::Col> column;
    std::shared_ptr<ast::SetClause> set_clause;
    std::vector<std::shared_ptr<ast::SetClause>> set_clauses;
    std::shared_ptr<ast::BinaryExpr> condition;
    std::shared_ptr<ast::FromNode> from_node;
    std::shared_ptr<ast::OrderBy> order_by;
    std::vector<std::shared_ptr<ast::OrderBy>> order_by_items;
    ast::OrderByDir order_direction = ast::OrderByDir::Default;
};

/**
 * @brief 一次解析调用中经由 yyparse() 传递的结果与诊断状态。
 * @internal
 *
 * RecordError() 只保留最早出现的错误，因为 Lexer 往往能在 Bison 生成通用的
 * `unexpected INVALID` 之前给出更准确的原因。块注释起点用于在注释未闭合时
 * 指向注释开始位置，而不是只报告文件末尾。
 */
struct ParseContext {
    std::shared_ptr<ast::Statement> statement;
    std::optional<ParseError> error;
    int block_comment_line = 1;
    int block_comment_column = 1;

    void RecordError(int line, int column, std::string message) {
        if (!error.has_value()) {
            error = ParseError{.line = line, .column = column, .message = std::move(message)};
        }
    }
};

/**
 * @brief 转换一个完整的整数 token，并防止溢出。
 * @param text token 字节序列，不要求以 NUL 结尾。
 * @param length @p text 的字节数。
 * @param value 成功时接收解析后的值。
 * @return 完整 token 能表示为 int 时返回 true。
 */
bool ParseIntegerLiteral(const char* text, int length, int* value);

/**
 * @brief 转换一个完整的浮点数 token，并防止溢出。
 * @param text token 字节序列，不要求以 NUL 结尾。
 * @param length @p text 的字节数。
 * @param value 成功时接收解析后的值。
 * @return 完整 token 能表示为 float 时返回 true。
 */
bool ParseFloatLiteral(const char* text, int length, float* value);

/**
 * @brief 去除首尾引号，并解码 SQL 双单引号转义（`''`）。
 * @param text 包含首尾引号的完整 token。
 * @param length @p text 的字节数，包括首尾两个引号。
 * @return 用于写入 AST 的解码后字符串。
 */
std::string DecodeStringLiteral(const char* text, int length);

}  // namespace rucbase::parser

#define YYSTYPE rucbase::parser::SemanticValue
