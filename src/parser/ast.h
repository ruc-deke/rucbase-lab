// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

/**
 * @file ast.h
 * @brief 定义 SQL 解析器生成的抽象语法树（AST）。
 *
 * Parser 只负责语法分析；表查找、列绑定和类型检查属于 Analyze 层，
 * 因此不应出现在这些节点中。
 */

#pragma once

#include <memory>
#include <string>
#include <vector>


namespace ast {

/** @brief SQL 字面量和列定义支持的数据类型。 */
enum SvType {
    SV_TYPE_INT,    ///< 32 位有符号整数。
    SV_TYPE_FLOAT,  ///< 单精度浮点数。
    SV_TYPE_STRING  ///< 定长字符数据。
};

/** @brief WHERE 子句支持的比较运算符。 */
enum SvCompOp {
    SV_OP_EQ,  ///< 等于（=）。
    SV_OP_NE,  ///< 不等于（<> 或 !=）。
    SV_OP_LT,  ///< 小于（<）。
    SV_OP_GT,  ///< 大于（>）。
    SV_OP_LE,  ///< 小于等于（<=）。
    SV_OP_GE   ///< 大于等于（>=）。
};

/** @brief ORDER BY 子句的排序方向。 */
enum OrderByDir {
    OrderBy_DEFAULT,  ///< 未显式指定方向，默认按升序处理。
    OrderBy_ASC,
    OrderBy_DESC
};

/**
 * @brief 显式 JOIN 子句的连接类型。
 *
 * 连接类型属于 SQL 的语法语义：Parser 在后续支持 INNER、LEFT、RIGHT 和
 * FULL JOIN 时将其写入 AST，Analyze 与 Planner 只消费解析结果。
 */
enum class JoinType {
    Inner,
    Left,
    Right,
    Full,
};

/** @brief 所有完整 SQL 语句 AST 节点的多态基类。 */
struct Statement {
    virtual ~Statement() = default;
};

/** @brief HELP 辅助语句。 */
struct Help : public Statement {
};

/** @brief SHOW TABLES 辅助语句。 */
struct ShowTables : public Statement {
};

/** @brief SHOW DATABASE 辅助语句，返回当前打开的数据库名。 */
struct ShowDatabase : public Statement {
};

/** @brief BEGIN 事务语句。 */
struct TxnBegin : public Statement {
};

/** @brief COMMIT 事务语句。 */
struct TxnCommit : public Statement {
};

/** @brief ABORT 事务语句。 */
struct TxnAbort : public Statement {
};

/** @brief ROLLBACK 事务语句。 */
struct TxnRollback : public Statement {
};

/** @brief 解析后的列类型及其物理字节长度。 */
struct TypeLen {
    SvType type = SV_TYPE_INT;
    int len = 0;

    TypeLen() = default;
    explicit TypeLen(const SvType type_, const int len_) : type(type_), len(len_) {}
};

/** @brief CREATE TABLE 中的一项列定义。 */
struct ColDef {
    std::string col_name;
    TypeLen type_len;

    ColDef() = default;
    explicit ColDef(std::string col_name_, const TypeLen type_len_) :
            col_name(std::move(col_name_)), type_len(type_len_) {}
};

/** @brief CREATE TABLE 语句及按声明顺序保存的列定义。 */
struct CreateTable : public Statement {
    std::string tab_name;
    std::vector<ColDef> fields;

    explicit CreateTable(std::string tab_name_, std::vector<ColDef> fields_) :
            tab_name(std::move(tab_name_)), fields(std::move(fields_)) {}
};

/** @brief DROP TABLE 语句。 */
struct DropTable : public Statement {
    std::string tab_name;

    explicit DropTable(std::string tab_name_) : tab_name(std::move(tab_name_)) {}
};

/** @brief 用于显示表结构的 DESC 语句。 */
struct DescTable : public Statement {
    std::string tab_name;

    explicit DescTable(std::string tab_name_) : tab_name(std::move(tab_name_)) {}
};

/** @brief 在有序列集合上创建索引的 CREATE INDEX 语句。 */
struct CreateIndex : public Statement {
    std::string tab_name;
    std::vector<std::string> col_names;

    explicit CreateIndex(std::string tab_name_, std::vector<std::string> col_names_) :
            tab_name(std::move(tab_name_)), col_names(std::move(col_names_)) {}
};

/** @brief 删除指定列集合索引的 DROP INDEX 语句。 */
struct DropIndex : public Statement {
    std::string tab_name;
    std::vector<std::string> col_names;

    explicit DropIndex(std::string tab_name_, std::vector<std::string> col_names_) :
            tab_name(std::move(tab_name_)), col_names(std::move(col_names_)) {}
};

/** @brief 谓词表达式 AST 节点的多态基类。 */
struct Expr {
    virtual ~Expr() = default;
};

/** @brief 字面量 AST 节点的多态基类。 */
struct Value : public Expr {
};

/** @brief 整数字面量。 */
struct IntLit : public Value {
    int val;

    explicit IntLit(int val_) : val(val_) {}
};

/** @brief 浮点数字面量。 */
struct FloatLit : public Value {
    float val;

    explicit FloatLit(float val_) : val(val_) {}
};

/** @brief 已完成 SQL 双单引号转义解码的字符串字面量。 */
struct StringLit : public Value {
    std::string val;

    explicit StringLit(std::string val_) : val(std::move(val_)) {}
};

/**
 * @brief 列引用，可以带表名前缀，也可以省略表名。
 *
 * @c tab_name 为空表示这是未限定列名，后续由 Analyze 完成列绑定。
 */
struct Col : public Expr {
    std::string tab_name;
    std::string col_name;

    explicit Col(std::string tab_name_, std::string col_name_) :
            tab_name(std::move(tab_name_)), col_name(std::move(col_name_)) {}
};

/** @brief UPDATE SET 子句中的一项赋值。 */
struct SetClause {
    std::string col_name;
    std::shared_ptr<Value> val;

    explicit SetClause(std::string col_name_, std::shared_ptr<Value> val_) :
            col_name(std::move(col_name_)), val(std::move(val_)) {}
};

/**
 * @brief 二元谓词，例如 `student.id = grade.student_id`。
 *
 * 语法要求左操作数必须是列；右操作数可以是字面量，也可以是另一个列引用。
 */
struct BinaryExpr {
    std::shared_ptr<Col> lhs;
    SvCompOp op;
    std::shared_ptr<Expr> rhs;

    explicit BinaryExpr(std::shared_ptr<Col> lhs_, SvCompOp op_, std::shared_ptr<Expr> rhs_) :
            lhs(std::move(lhs_)), op(op_), rhs(std::move(rhs_)) {}
};

/** @brief ORDER BY 的排序列与可选方向。 */
struct OrderBy {
    std::shared_ptr<Col> column;
    OrderByDir direction;
    explicit OrderBy(std::shared_ptr<Col> column_, OrderByDir direction_) :
       column(std::move(column_)), direction(direction_) {}
};

/** @brief INSERT 语句及按源码顺序保存的值。 */
struct InsertStmt : public Statement {
    std::string tab_name;
    std::vector<std::shared_ptr<Value>> vals;

    explicit InsertStmt(std::string tab_name_, std::vector<std::shared_ptr<Value>> vals_) :
            tab_name(std::move(tab_name_)), vals(std::move(vals_)) {}
};

/** @brief DELETE 语句及可选的合取谓词。 */
struct DeleteStmt : public Statement {
    std::string tab_name;
    std::vector<std::shared_ptr<BinaryExpr>> conds;

    explicit DeleteStmt(std::string tab_name_, std::vector<std::shared_ptr<BinaryExpr>> conds_) :
            tab_name(std::move(tab_name_)), conds(std::move(conds_)) {}
};

/** @brief UPDATE 语句、赋值列表及可选谓词。 */
struct UpdateStmt : public Statement {
    std::string tab_name;
    std::vector<std::shared_ptr<SetClause>> set_clauses;
    std::vector<std::shared_ptr<BinaryExpr>> conds;

    explicit UpdateStmt(std::string tab_name_,
               std::vector<std::shared_ptr<SetClause>> set_clauses_,
               std::vector<std::shared_ptr<BinaryExpr>> conds_) :
            tab_name(std::move(tab_name_)), set_clauses(std::move(set_clauses_)), conds(std::move(conds_)) {}
};

/**
 * @brief 尚未经过目录相关语义分析的 SELECT 语句。
 *
 * 此处按解析结果保存表名和列名；后续由 Analyze 解析未限定列名、检查歧义，
 * 并验证被引用的表和列。
 */
struct SelectStmt : public Statement {
    /** 空列表表示投影项为 `*`。 */
    std::vector<std::shared_ptr<Col>> cols;
    std::vector<std::string> tabs;
    std::vector<std::shared_ptr<BinaryExpr>> conds;
    /** nullptr 表示语句没有 ORDER BY 子句。 */
    std::shared_ptr<OrderBy> order;
    explicit SelectStmt(std::vector<std::shared_ptr<Col>> cols_,
               std::vector<std::string> tabs_,
               std::vector<std::shared_ptr<BinaryExpr>> conds_,
               std::shared_ptr<OrderBy> order_) :
            cols(std::move(cols_)), tabs(std::move(tabs_)), conds(std::move(conds_)), 
            order(std::move(order_)) {}
};

}  // namespace ast
