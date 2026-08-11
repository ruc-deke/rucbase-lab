// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

/**
 * @file parser.h
 * @brief 将一条 SQL 语句转换为 AST 的公共入口。
 *
 * Flex/Bison 的类型和函数均隐藏在模块内部。调用方只需通过 Parse() 提交
 * SQL，并获得一棵语句 AST 或一项结构化错误。
 */

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "parser/ast.h"

namespace rucbase::parser {

/** @brief 词法错误或语法错误的位置与原因。 */
struct ParseError {
    int line = 1;         ///< 从 1 开始计数的源码行号。
    int column = 1;       ///< 从 1 开始计数的 UTF-8 字节列号。
    std::string message;  ///< 不包含源码上下文的错误说明。
};

/**
 * @brief 一次 Parse() 调用的结果。
 *
 * @c statement 与 @c error 中只会有一个有效。调用方应使用 ok() 判断结果，
 * 不必在每个调用点分别检查两个成员。
 */
struct ParseResult {
    std::shared_ptr<ast::Statement> statement;
    std::optional<ParseError> error;

    /** @return 成功解析出一条完整语句时返回 true。 */
    [[nodiscard]] bool ok() const noexcept { return statement != nullptr && !error.has_value(); }
};

/**
 * @brief 解析且只解析一条以分号结束的 SQL 语句。
 *
 * 必须完整消费全部输入；尾随 token 或第二条语句都会被拒绝。该函数可以被
 * 多线程安全调用，但当前不可重入的 Flex scanner 会在模块内部串行执行。
 *
 * @param sql SQL 文本；不允许包含 NUL 字节。
 * @return 成功时返回语句 AST，失败时返回结构化解析错误。
 * @throws std::runtime_error 无法分配 scanner buffer 时抛出。
 */
ParseResult Parse(std::string_view sql);

/**
 * @brief 为结构化解析错误补充源码行和插入符号定位。
 *
 * @param sql 传给 Parse() 的原始 SQL 文本。
 * @param error Parse() 返回的错误。
 * @return 以换行结束、适合交互式客户端显示的诊断文本。
 */
std::string FormatError(std::string_view sql, const ParseError& error);

}  // namespace rucbase::parser
