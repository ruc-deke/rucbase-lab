/**
 * @file yacc.y
 * @brief 将 Bison token 组合成一条 SQL 语句 AST 的语法规则。
 *
 * 语法动作只负责构造语法对象；目录查找、列绑定和类型兼容性检查由 Analyzer
 * 完成。解析成功要求输入中恰好有一条以分号结束的语句，并且随后到达真正的
 * 输入末尾。
 *
 * Copyright (c) 2023-2027 Renmin University of China
 * SPDX-License-Identifier: MulanPSL-2.0
 */

%{
#include "parser/parser_internal.h"
#include "yacc.tab.h"
#include <memory>

int yylex(YYSTYPE *yylval, YYLTYPE *yylloc, rucbase::parser::ParseContext *context);

/** @brief 将 Bison 报告的首个语法错误写入本次解析上下文。 */
void yyerror(YYLTYPE *locp, rucbase::parser::ParseContext *context, const char* message) {
    context->RecordError(locp->first_line, locp->first_column,
                         message == nullptr ? "syntax error" : message);
}

using namespace ast;
%}

%code requires {
#include "parser/parser_internal.h"
}

// Bison 的解析状态是局部的；不可重入的 Flex scanner 在 parser.cpp 内部
// 串行执行，并且不会暴露给调用方。
%define api.pure full
// 启用源码位置跟踪。
%locations
// 生成包含实际 token 和期望 token 的详细语法错误。
%define parse.error verbose
%parse-param {rucbase::parser::ParseContext *context}
%lex-param {rucbase::parser::ParseContext *context}

// SQL 关键字。
%token SHOW DATABASE TABLES CREATE TABLE DROP DESC INSERT INTO VALUES DELETE FROM ASC ORDER BY
WHERE UPDATE SET SELECT INT CHAR FLOAT INDEX UNIQUE AND JOIN HELP TXN_BEGIN TXN_COMMIT TXN_ABORT TXN_ROLLBACK
// 运算符和错误 token。
%token LEQ NEQ GEQ INVALID

// 携带语义值的 token。
%token <text> IDENTIFIER VALUE_STRING
%token <integer> VALUE_INT
%token <floating_point> VALUE_FLOAT

// 指定各非终结符使用的 SemanticValue 成员。
%type <statement> stmt utility_stmt ddl_stmt dml_stmt transaction_stmt
%type <column_definition> column_definition
%type <column_definitions> column_definition_list
%type <type_length> column_type
%type <comparison_operator> comparison_operator
%type <expression> comparison_operand scalar_expression function_call predicate optional_where select_item
%type <expressions> select_list select_item_list scalar_expression_list
%type <value> value
%type <values> value_list
%type <text> table_name column_name
%type <column_names> column_name_list
%type <from_node> from_tree
%type <column> column_reference
%type <set_clause> set_clause
%type <set_clauses> set_clause_list
%type <condition> comparison
%type <order_by> order_item
%type <order_by_items> optional_order_by
%type <order_direction> optional_order_direction

%%
start:
        stmt ';'
    {
        context->statement = std::move($1);
    }
    ;

stmt:
        utility_stmt
    |   ddl_stmt
    |   dml_stmt
    |   transaction_stmt
    ;

transaction_stmt:
        TXN_BEGIN
    {
        $$ = std::make_shared<TxnBeginStmt>();
    }
    |   TXN_COMMIT
    {
        $$ = std::make_shared<TxnCommitStmt>();
    }
    |   TXN_ABORT
    {
        $$ = std::make_shared<TxnAbortStmt>();
    }
    |   TXN_ROLLBACK
    {
        $$ = std::make_shared<TxnRollbackStmt>();
    }
    ;

utility_stmt:
        SHOW DATABASE
    {
        $$ = std::make_shared<ShowDatabaseStmt>();
    }
    |   SHOW TABLES
    {
        $$ = std::make_shared<ShowTablesStmt>();
    }
    |   HELP
    {
        $$ = std::make_shared<HelpStmt>();
    }
    |   SET IDENTIFIER '=' IDENTIFIER
    {
        $$ = std::make_shared<SetKnobStmt>(std::move($2), std::move($4));
    }
    ;

ddl_stmt:
        CREATE TABLE table_name '(' column_definition_list ')'
    {
        $$ = std::make_shared<CreateTableStmt>(std::move($3), std::move($5));
    }
    |   DROP TABLE table_name
    {
        $$ = std::make_shared<DropTableStmt>(std::move($3));
    }
    |   DESC table_name
    {
        $$ = std::make_shared<DescTableStmt>(std::move($2));
    }
    |   CREATE INDEX table_name '(' column_name_list ')'
    {
        $$ = std::make_shared<CreateIndexStmt>(std::move($3), std::move($5), false);
    }
    |   CREATE UNIQUE INDEX table_name '(' column_name_list ')'
    {
        $$ = std::make_shared<CreateIndexStmt>(std::move($4), std::move($6), true);
    }
    |   DROP INDEX table_name '(' column_name_list ')'
    {
        $$ = std::make_shared<DropIndexStmt>(std::move($3), std::move($5));
    }
    ;

dml_stmt:
        INSERT INTO table_name VALUES '(' value_list ')'
    {
        $$ = std::make_shared<InsertStmt>(std::move($3), std::move($6));
    }
    |   DELETE FROM table_name optional_where
    {
        $$ = std::make_shared<DeleteStmt>(std::move($3), std::move($4));
    }
    |   UPDATE table_name SET set_clause_list optional_where
    {
        $$ = std::make_shared<UpdateStmt>(std::move($2), std::move($4), std::move($5));
    }
    |   SELECT select_list FROM from_tree optional_where optional_order_by
    {
        $$ = std::make_shared<SelectStmt>(std::move($2), std::move($4), std::move($5), std::move($6));
    }
    ;

column_definition_list:
        column_definition
    {
        $$ = std::vector<ColDef>{std::move($1)};
    }
    |   column_definition_list ',' column_definition
    {
        $$.push_back(std::move($3));
    }
    ;

column_name_list:
        column_name
    {
        $$ = std::vector<std::string>{std::move($1)};
    }
    |   column_name_list ',' column_name
    {
        $$.push_back(std::move($3));
    }
    ;

column_definition:
        column_name column_type
    {
        $$ = ColDef(std::move($1), $2);
    }
    ;

column_type:
        INT
    {
        $$ = TypeLen(DataType::Int, sizeof(int));
    }
    |   CHAR '(' VALUE_INT ')'
    {
        $$ = TypeLen(DataType::String, $3);
    }
    |   FLOAT
    {
        $$ = TypeLen(DataType::Float, sizeof(float));
    }
    ;

value_list:
        value
    {
        $$ = std::vector<std::shared_ptr<Value>>{std::move($1)};
    }
    |   value_list ',' value
    {
        $$.push_back(std::move($3));
    }
    ;

value:
        VALUE_INT
    {
        $$ = std::make_shared<IntLit>($1);
    }
    |   VALUE_FLOAT
    {
        $$ = std::make_shared<FloatLit>($1);
    }
    |   VALUE_STRING
    {
        $$ = std::make_shared<StringLit>(std::move($1));
    }
    ;

comparison:
        column_reference comparison_operator comparison_operand
    {
        $$ = std::make_shared<BinaryExpr>(std::move($1), $2, std::move($3));
    }
    ;

optional_where:
        %empty
    {
        $$ = nullptr;
    }
    |   WHERE predicate
    {
        $$ = std::move($2);
    }
    ;

predicate:
        comparison
    {
        $$ = std::move($1);
    }
    |   predicate AND comparison
    {
        $$ = std::make_shared<LogicalExpr>(LogicalOp::And, std::move($1), std::move($3));
    }
    ;

column_reference:
        table_name '.' column_name
    {
        $$ = std::make_shared<Col>(std::move($1), std::move($3));
    }
    |   column_name
    {
        $$ = std::make_shared<Col>("", std::move($1));
    }
    ;

comparison_operator:
        '='
    {
        $$ = CompOp::Eq;
    }
    |   '<'
    {
        $$ = CompOp::Lt;
    }
    |   '>'
    {
        $$ = CompOp::Gt;
    }
    |   NEQ
    {
        $$ = CompOp::Ne;
    }
    |   LEQ
    {
        $$ = CompOp::Le;
    }
    |   GEQ
    {
        $$ = CompOp::Ge;
    }
    ;

comparison_operand:
        value
    {
        $$ = std::move($1);
    }
    |   column_reference
    {
        $$ = std::move($1);
    }
    ;

scalar_expression:
        value
    {
        $$ = std::move($1);
    }
    |   column_reference
    {
        $$ = std::move($1);
    }
    |   function_call
    {
        $$ = std::move($1);
    }
    |   '(' scalar_expression ')'
    {
        $$ = std::move($2);
    }
    ;

scalar_expression_list:
        scalar_expression
    {
        $$ = std::vector<std::shared_ptr<Expr>>{std::move($1)};
    }
    |   scalar_expression_list ',' scalar_expression
    {
        $$.push_back(std::move($3));
    }
    ;

function_call:
        IDENTIFIER '(' ')'
    {
        $$ = std::make_shared<FunctionCallExpr>(std::move($1), std::vector<std::shared_ptr<Expr>>{});
    }
    |   IDENTIFIER '(' scalar_expression_list ')'
    {
        $$ = std::make_shared<FunctionCallExpr>(std::move($1), std::move($3));
    }
    |   IDENTIFIER '(' '*' ')'
    {
        std::vector<std::shared_ptr<Expr>> arguments;
        arguments.push_back(std::make_shared<StarExpr>());
        $$ = std::make_shared<FunctionCallExpr>(std::move($1), std::move(arguments));
    }
    ;

set_clause_list:
        set_clause
    {
        $$ = std::vector<std::shared_ptr<SetClause>>{std::move($1)};
    }
    |   set_clause_list ',' set_clause
    {
        $$.push_back(std::move($3));
    }
    ;

set_clause:
        column_reference '=' scalar_expression
    {
        $$ = std::make_shared<SetClause>(std::move($1), std::move($3));
    }
    ;

select_item:
        column_reference
    {
        $$ = std::move($1);
    }
    ;

select_item_list:
        select_item
    {
        $$ = std::vector<std::shared_ptr<Expr>>{std::move($1)};
    }
    |   select_item_list ',' select_item
    {
        $$.push_back(std::move($3));
    }
    ;

select_list:
        '*'
    {
        $$ = std::vector<std::shared_ptr<Expr>>{std::make_shared<StarExpr>()};
    }
    |   select_item_list
    {
        $$ = std::move($1);
    }
    ;

from_tree:
        table_name
    {
        $$ = std::make_shared<TableRef>(std::move($1));
    }
    |   from_tree ',' table_name
    {
        $$ = std::make_shared<JoinNode>(JoinType::Cross, std::move($1),
                                        std::make_shared<TableRef>(std::move($3)));
    }
    |   from_tree JOIN table_name
    {
        $$ = std::make_shared<JoinNode>(JoinType::Inner, std::move($1),
                                        std::make_shared<TableRef>(std::move($3)));
    }
    ;

optional_order_by:
        ORDER BY order_item
    { 
        $$ = std::vector<std::shared_ptr<OrderBy>>{std::move($3)};
    }
    |   %empty
    {
        $$ = {};
    }
    ;

order_item:
        column_reference optional_order_direction
    { 
        $$ = std::make_shared<OrderBy>(std::move($1), $2);
    }
    ;   

optional_order_direction:
        ASC             { $$ = OrderByDir::Asc; }
    |   DESC            { $$ = OrderByDir::Desc; }
    |   %empty          { $$ = OrderByDir::Default; }
    ;

table_name:
    IDENTIFIER { $$ = std::move($1); }
    ;

column_name:
    IDENTIFIER { $$ = std::move($1); }
    ;
%%
