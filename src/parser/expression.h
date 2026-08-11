// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

/**
 * @file expression.h
 * @brief 定义 SQL AST 的表达式节点。
 *
 * 表达式只保存 Parser 可从 SQL 文本确定的语法信息。列绑定、类型推导、
 * 聚合合法性和子查询相关性检查仍由 Analyzer 负责。
 */

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace ast {

struct SelectStmt;

/** @brief 比较谓词支持的运算符。 */
enum class CompOp {
    Eq,  ///< 等于（=）。
    Ne,  ///< 不等于（<> 或 !=）。
    Lt,  ///< 小于（<）。
    Gt,  ///< 大于（>）。
    Le,  ///< 小于等于（<=）。
    Ge,  ///< 大于等于（>=）。
};

/** @brief WHERE/HAVING 表达式中的二元逻辑运算符。 */
enum class LogicalOp { And, Or };

/** @brief WHERE/HAVING 表达式中的一元逻辑运算符。 */
enum class UnaryOp { Not };

/** @brief 表达式节点类别，供 Analyzer 做无 RTTI 的穷尽分派。 */
enum class ExprKind {
    IntLiteral,
    FloatLiteral,
    StringLiteral,
    Column,
    Star,
    Binary,
    Logical,
    Unary,
    FunctionCall,
    Subquery,
};

/** @brief 所有 SQL 表达式 AST 节点的多态基类。 */
struct Expr {
    virtual ~Expr() = default;

    ExprKind kind() const { return kind_; }

protected:
    explicit Expr(const ExprKind kind) : kind_(kind) {}

private:
    ExprKind kind_;
};

/** @brief 字面量节点的分类基类，便于 INSERT、UPDATE 等语法限制值类型。 */
struct Value : public Expr {
protected:
    explicit Value(const ExprKind kind) : Expr(kind) {}
};

/** @brief 整数字面量。 */
struct IntLit : public Value {
    int val;  ///< Lexer 已完成范围检查的整数值。

    explicit IntLit(const int val_) : Value(ExprKind::IntLiteral), val(val_) {}
};

/** @brief 浮点数字面量。 */
struct FloatLit : public Value {
    float val;  ///< Lexer 已完成范围检查的单精度值。

    explicit FloatLit(const float val_) : Value(ExprKind::FloatLiteral), val(val_) {}
};

/** @brief 已完成 SQL 双单引号转义解码的字符串字面量。 */
struct StringLit : public Value {
    std::string val;  ///< 解码后的字符串内容，不包含 SQL 引号。

    explicit StringLit(std::string val_) : Value(ExprKind::StringLiteral), val(std::move(val_)) {}
};

/**
 * @brief 可限定或未限定的列引用。
 *
 * @c tab_name 为空表示未限定列名，后续由 Analyzer 在当前 FROM 范围内绑定。
 */
struct Col : public Expr {
    std::string tab_name;  ///< 可选的表限定名；空字符串表示未限定。
    std::string col_name;  ///< SQL 中声明的列名。

    Col(std::string tab_name_, std::string col_name_)
        : Expr(ExprKind::Column),
          tab_name(std::move(tab_name_)),
          col_name(std::move(col_name_)) {}
};

/** @brief SELECT *、table.* 或 COUNT(*) 中的星号表达式。 */
struct StarExpr : public Expr {
    std::string tab_name;  ///< 空表示未限定 `*`，非空表示 `table.*`。

    explicit StarExpr(std::string tab_name_ = {}) : Expr(ExprKind::Star), tab_name(std::move(tab_name_)) {}
};

/**
 * @brief 二元比较谓词，例如 `student.id = grade.student_id`。
 *
 * 当前语法要求左操作数必须是列；右操作数可以是字面量或列。
 */
struct BinaryExpr : public Expr {
    std::shared_ptr<Col> lhs;   ///< 当前语法限定的左侧列引用。
    CompOp op;                  ///< 比较运算符。
    std::shared_ptr<Expr> rhs;  ///< 比较右操作数。

    BinaryExpr(std::shared_ptr<Col> lhs_, const CompOp op_, std::shared_ptr<Expr> rhs_)
        : Expr(ExprKind::Binary),
          lhs(std::move(lhs_)),
          op(op_),
          rhs(std::move(rhs_)) {}
};

/** @brief AND/OR 组成的逻辑表达式，可用于 WHERE 或 HAVING。 */
struct LogicalExpr : public Expr {
    LogicalOp op;               ///< AND 或 OR。
    std::shared_ptr<Expr> lhs;  ///< 左侧布尔表达式。
    std::shared_ptr<Expr> rhs;  ///< 右侧布尔表达式。

    LogicalExpr(const LogicalOp op_, std::shared_ptr<Expr> lhs_, std::shared_ptr<Expr> rhs_)
        : Expr(ExprKind::Logical),
          op(op_),
          lhs(std::move(lhs_)),
          rhs(std::move(rhs_)) {}
};

/** @brief NOT 等一元逻辑表达式。 */
struct UnaryExpr : public Expr {
    UnaryOp op;                     ///< 一元逻辑运算符。
    std::shared_ptr<Expr> operand;  ///< 被运算的布尔表达式。

    UnaryExpr(const UnaryOp op_, std::shared_ptr<Expr> operand_)
        : Expr(ExprKind::Unary),
          op(op_),
          operand(std::move(operand_)) {}
};

/** @brief SUM 等函数或聚合调用；名称保留原始 SQL 标识符。 */
struct FunctionCallExpr : public Expr {
    std::string name;                              ///< 源码中的函数名。
    std::vector<std::shared_ptr<Expr>> arguments;  ///< 按源码顺序保存的实参。
    bool distinct = false;                         ///< 是否带 DISTINCT 修饰。

    FunctionCallExpr(std::string name_, std::vector<std::shared_ptr<Expr>> arguments_, const bool distinct_ = false)
        : Expr(ExprKind::FunctionCall),
          name(std::move(name_)),
          arguments(std::move(arguments_)),
          distinct(distinct_) {}
};

/** @brief 标量或集合子查询表达式；具体语义由使用上下文决定。 */
struct SubqueryExpr : public Expr {
    std::shared_ptr<SelectStmt> query;  ///< 子查询的 SELECT AST 根节点。

    explicit SubqueryExpr(std::shared_ptr<SelectStmt> query_) : Expr(ExprKind::Subquery), query(std::move(query_)) {}
};

}  // namespace ast
