// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

/**
 * @file ast.h
 * @brief 定义 SQL 语句、公共子句和 FROM/JOIN 语法树。
 *
 * 表达式节点单独放在 expression.h。Parser 只记录语法结构；表查找、
 * 列绑定和类型检查由 Analyzer 完成。
 */

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "parser/expression.h"

namespace ast {

/** @brief CREATE TABLE 列定义支持的数据类型。 */
enum class DataType {
    Int,     ///< 32 位有符号整数。
    Float,   ///< 单精度浮点数。
    String,  ///< 定长字符数据。
};

/** @brief 完整 SQL 语句的类别。 */
enum class StatementKind {
    Help,
    ShowTables,
    ShowDatabase,
    SetKnob,
    TxnBegin,
    TxnCommit,
    TxnAbort,
    TxnRollback,
    CreateTable,
    DropTable,
    DescTable,
    CreateIndex,
    DropIndex,
    Insert,
    Delete,
    Update,
    Select,
};

/** @brief 所有完整 SQL 语句的共同基类。 */
struct Statement {
    virtual ~Statement() = default;

    StatementKind kind() const { return kind_; }

protected:
    explicit Statement(const StatementKind kind) : kind_(kind) {}

private:
    StatementKind kind_;
};

/** @brief ORDER BY 的排序方向。 */
enum class OrderByDir {
    Default,
    Asc,
    Desc,
};

/** @brief FROM 中的连接类型。逗号连接记为 Cross。 */
enum class JoinType {
    Cross,
    Inner,
    Left,
    Right,
    Full,
};

/** @brief FROM 树中的节点类别。 */
enum class FromNodeKind {
    Table,
    Join,
};

/** @brief 跟随在From后面的表引用和 JOIN 节点的共同基类。 */
struct FromNode {
    virtual ~FromNode() = default;

    FromNodeKind kind() const { return kind_; }

protected:
    explicit FromNode(const FromNodeKind kind) : kind_(kind) {}

private:
    FromNodeKind kind_;
};

// CREATE TABLE、UPDATE、ORDER BY 等语句共用的小型语法结构。

struct TypeLen {
    DataType type = DataType::Int;
    int declared_len = 0;

    TypeLen() = default;
    TypeLen(const DataType type_, const int declared_len_) : type(type_), declared_len(declared_len_) {}
};

struct ColDef {
    std::string col_name;
    TypeLen type_len;

    ColDef() = default;
    ColDef(std::string col_name_, const TypeLen type_len_) : col_name(std::move(col_name_)), type_len(type_len_) {}
};

/** @brief UPDATE 中的一项赋值，例如 `student.score = ABS(student.score)`。 */
struct SetClause {
    std::shared_ptr<Col> target_column;   ///< 赋值目标，支持 `column` 和 `table.column`。
    std::shared_ptr<Expr> assigned_expr;  ///< 右侧标量表达式；具体支持范围由 Analyzer 决定。

    SetClause(std::shared_ptr<Col> target_column_, std::shared_ptr<Expr> assigned_expr_)
        : target_column(std::move(target_column_)),
          assigned_expr(std::move(assigned_expr_)) {}
};

struct OrderBy {
    std::shared_ptr<Expr> expression;
    OrderByDir direction = OrderByDir::Default;

    OrderBy(std::shared_ptr<Expr> expression_, const OrderByDir direction_)
        : expression(std::move(expression_)),
          direction(direction_) {}
};

struct LimitClause {
    std::int64_t count = 0;              ///< 用户声明的行数，留待 Analyzer 检查非负。
    std::optional<std::int64_t> offset;  ///< 可选偏移量，同样保留有符号输入。
};

// FROM 使用递归树，因此左右孩子都可以继续是 TableRef 或 JoinNode。

struct TableRef : public FromNode {
    std::variant<std::string, std::shared_ptr<SelectStmt>> source;
    std::string alias;

    TableRef() = delete;
    explicit TableRef(std::string table_name, std::string alias_ = {})
        : FromNode(FromNodeKind::Table),
          source(std::move(table_name)),
          alias(std::move(alias_)) {}
    TableRef(std::shared_ptr<SelectStmt> subquery, std::string alias_)
        : FromNode(FromNodeKind::Table),
          source(std::move(subquery)),
          alias(std::move(alias_)) {}
};

struct JoinNode : public FromNode {
    JoinType type;
    std::shared_ptr<FromNode> left;
    std::shared_ptr<FromNode> right;
    std::shared_ptr<Expr> condition;  ///< nullptr 表示没有 ON 条件。

    JoinNode(const JoinType type_,
             std::shared_ptr<FromNode> left_,
             std::shared_ptr<FromNode> right_,
             std::shared_ptr<Expr> condition_ = nullptr)
        : FromNode(FromNodeKind::Join),
          type(type_),
          left(std::move(left_)),
          right(std::move(right_)),
          condition(std::move(condition_)) {}
};

// 不携带额外数据的辅助语句和事务语句。

struct HelpStmt : public Statement {
    HelpStmt() : Statement(StatementKind::Help) {}
};

struct ShowTablesStmt : public Statement {
    ShowTablesStmt() : Statement(StatementKind::ShowTables) {}
};

struct ShowDatabaseStmt : public Statement {
    ShowDatabaseStmt() : Statement(StatementKind::ShowDatabase) {}
};

/** @brief `SET name = value`：修改服务端的查询设置，例如 `SET enable_sortmerge = true`。 */
struct SetKnobStmt : public Statement {
    std::string name;
    std::string value;

    SetKnobStmt(std::string name_, std::string value_)
        : Statement(StatementKind::SetKnob),
          name(std::move(name_)),
          value(std::move(value_)) {}
};

struct TxnBeginStmt : public Statement {
    TxnBeginStmt() : Statement(StatementKind::TxnBegin) {}
};

struct TxnCommitStmt : public Statement {
    TxnCommitStmt() : Statement(StatementKind::TxnCommit) {}
};

struct TxnAbortStmt : public Statement {
    TxnAbortStmt() : Statement(StatementKind::TxnAbort) {}
};

struct TxnRollbackStmt : public Statement {
    TxnRollbackStmt() : Statement(StatementKind::TxnRollback) {}
};

// DDL 语句。

struct CreateTableStmt : public Statement {
    std::string tab_name;
    std::vector<ColDef> fields;

    CreateTableStmt(std::string tab_name_, std::vector<ColDef> fields_)
        : Statement(StatementKind::CreateTable),
          tab_name(std::move(tab_name_)),
          fields(std::move(fields_)) {}
};

struct DropTableStmt : public Statement {
    std::string tab_name;

    explicit DropTableStmt(std::string tab_name_)
        : Statement(StatementKind::DropTable),
          tab_name(std::move(tab_name_)) {}
};

struct DescTableStmt : public Statement {
    std::string tab_name;

    explicit DescTableStmt(std::string tab_name_)
        : Statement(StatementKind::DescTable),
          tab_name(std::move(tab_name_)) {}
};

struct CreateIndexStmt : public Statement {
    std::string tab_name;
    std::vector<std::string> col_names;
    bool unique;

    CreateIndexStmt(std::string tab_name_, std::vector<std::string> col_names_, bool unique_ = false)
        : Statement(StatementKind::CreateIndex),
          tab_name(std::move(tab_name_)),
          col_names(std::move(col_names_)),
          unique(unique_) {}
};

struct DropIndexStmt : public Statement {
    std::string tab_name;
    std::vector<std::string> col_names;

    DropIndexStmt(std::string tab_name_, std::vector<std::string> col_names_)
        : Statement(StatementKind::DropIndex),
          tab_name(std::move(tab_name_)),
          col_names(std::move(col_names_)) {}
};

// DML 语句。

struct InsertStmt : public Statement {
    std::string tab_name;
    std::vector<std::shared_ptr<Value>> values;

    InsertStmt(std::string tab_name_, std::vector<std::shared_ptr<Value>> values_)
        : Statement(StatementKind::Insert),
          tab_name(std::move(tab_name_)),
          values(std::move(values_)) {}
};

struct DeleteStmt : public Statement {
    std::string tab_name;
    std::shared_ptr<Expr> where;

    DeleteStmt(std::string tab_name_, std::shared_ptr<Expr> where_)
        : Statement(StatementKind::Delete),
          tab_name(std::move(tab_name_)),
          where(std::move(where_)) {}
};

struct UpdateStmt : public Statement {
    std::string tab_name;
    std::vector<std::shared_ptr<SetClause>> set_clauses;
    std::shared_ptr<Expr> where;

    UpdateStmt(std::string tab_name_,
               std::vector<std::shared_ptr<SetClause>> set_clauses_,
               std::shared_ptr<Expr> where_)
        : Statement(StatementKind::Update),
          tab_name(std::move(tab_name_)),
          set_clauses(std::move(set_clauses_)),
          where(std::move(where_)) {}
};

struct SelectStmt : public Statement {
    std::vector<std::shared_ptr<Expr>> select_items;
    std::shared_ptr<FromNode> from;
    std::shared_ptr<Expr> where;

    // GROUP BY、HAVING 和 LIMIT 尚未进入当前 Parser/执行层。
    std::vector<std::shared_ptr<Expr>> group_by;
    std::shared_ptr<Expr> having;
    // ORDER BY 已支持，但与其他 SELECT 尾部子句放在一起更容易对应 SQL 顺序。
    std::vector<std::shared_ptr<OrderBy>> order_by;
    std::optional<LimitClause> limit;

    SelectStmt(std::vector<std::shared_ptr<Expr>> select_items_,
               std::shared_ptr<FromNode> from_,
               std::shared_ptr<Expr> where_,
               std::vector<std::shared_ptr<OrderBy>> order_by_,
               std::vector<std::shared_ptr<Expr>> group_by_ = {},
               std::shared_ptr<Expr> having_ = nullptr,
               std::optional<LimitClause> limit_ = std::nullopt)
        : Statement(StatementKind::Select),
          select_items(std::move(select_items_)),
          from(std::move(from_)),
          where(std::move(where_)),
          group_by(std::move(group_by_)),
          having(std::move(having_)),
          order_by(std::move(order_by_)),
          limit(limit_) {}
};

}  // namespace ast
