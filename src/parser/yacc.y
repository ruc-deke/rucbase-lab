/**
 * @file yacc.y
 * @brief 将 Bison token 组合成一条 SQL 语句 AST 的语法规则。
 *
 * 语法动作只负责构造语法对象；目录查找、列绑定和类型兼容性检查由 Analyze
 * 完成。解析成功要求输入中恰好有一条以分号结束的语句，并且随后到达真正的
 * 输入末尾。
 *
 * Copyright (c) 2023-2026 Renmin University of China
 * SPDX-License-Identifier: MulanPSL-2.0
 */

%{
#include "parser_internal.h"
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
#include "parser_internal.h"
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
WHERE UPDATE SET SELECT INT CHAR FLOAT INDEX AND JOIN HELP TXN_BEGIN TXN_COMMIT TXN_ABORT TXN_ROLLBACK
// 运算符和错误 token。
%token LEQ NEQ GEQ INVALID

// 携带语义值的 token。
%token <sv_str> IDENTIFIER VALUE_STRING
%token <sv_int> VALUE_INT
%token <sv_float> VALUE_FLOAT

// 指定各非终结符使用的 SemanticValue 成员。
%type <sv_node> stmt dbStmt ddl dml txnStmt
%type <sv_field> field
%type <sv_fields> fieldList
%type <sv_type_len> type
%type <sv_comp_op> op
%type <sv_expr> expr
%type <sv_val> value
%type <sv_vals> valueList
%type <sv_str> tbName colName
%type <sv_strs> tableList colNameList
%type <sv_col> col
%type <sv_cols> colList selector
%type <sv_set_clause> setClause
%type <sv_set_clauses> setClauses
%type <sv_cond> condition
%type <sv_conds> whereClause optWhereClause
%type <sv_orderby>  order_clause opt_order_clause
%type <sv_orderby_dir> opt_asc_desc

%%
start:
        stmt ';'
    {
        context->statement = std::move($1);
    }
    ;

stmt:
        dbStmt
    |   ddl
    |   dml
    |   txnStmt
    ;

txnStmt:
        TXN_BEGIN
    {
        $$ = std::make_shared<TxnBegin>();
    }
    |   TXN_COMMIT
    {
        $$ = std::make_shared<TxnCommit>();
    }
    |   TXN_ABORT
    {
        $$ = std::make_shared<TxnAbort>();
    }
    | TXN_ROLLBACK
    {
        $$ = std::make_shared<TxnRollback>();
    }
    ;

dbStmt:
        SHOW DATABASE
    {
        $$ = std::make_shared<ShowDatabase>();
    }
    |   SHOW TABLES
    {
        $$ = std::make_shared<ShowTables>();
    }
    |   HELP
    {
        $$ = std::make_shared<Help>();
    }
    ;

ddl:
        CREATE TABLE tbName '(' fieldList ')'
    {
        $$ = std::make_shared<CreateTable>($3, $5);
    }
    |   DROP TABLE tbName
    {
        $$ = std::make_shared<DropTable>($3);
    }
    |   DESC tbName
    {
        $$ = std::make_shared<DescTable>($2);
    }
    |   CREATE INDEX tbName '(' colNameList ')'
    {
        $$ = std::make_shared<CreateIndex>($3, $5);
    }
    |   DROP INDEX tbName '(' colNameList ')'
    {
        $$ = std::make_shared<DropIndex>($3, $5);
    }
    ;

dml:
        INSERT INTO tbName VALUES '(' valueList ')'
    {
        $$ = std::make_shared<InsertStmt>($3, $6);
    }
    |   DELETE FROM tbName optWhereClause
    {
        $$ = std::make_shared<DeleteStmt>($3, $4);
    }
    |   UPDATE tbName SET setClauses optWhereClause
    {
        $$ = std::make_shared<UpdateStmt>($2, $4, $5);
    }
    |   SELECT selector FROM tableList optWhereClause opt_order_clause
    {
        $$ = std::make_shared<SelectStmt>($2, $4, $5, $6);
    }
    ;

fieldList:
        field
    {
        $$ = std::vector<ColDef>{std::move($1)};
    }
    |   fieldList ',' field
    {
        $$.push_back(std::move($3));
    }
    ;

colNameList:
        colName
    {
        $$ = std::vector<std::string>{$1};
    }
    | colNameList ',' colName
    {
        $$.push_back($3);
    }
    ;

field:
        colName type
    {
        $$ = ColDef(std::move($1), $2);
    }
    ;

type:
        INT
    {
        $$ = TypeLen(SV_TYPE_INT, sizeof(int));
    }
    |   CHAR '(' VALUE_INT ')'
    {
        $$ = TypeLen(SV_TYPE_STRING, $3);
    }
    |   FLOAT
    {
        $$ = TypeLen(SV_TYPE_FLOAT, sizeof(float));
    }
    ;

valueList:
        value
    {
        $$ = std::vector<std::shared_ptr<Value>>{$1};
    }
    |   valueList ',' value
    {
        $$.push_back($3);
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
        $$ = std::make_shared<StringLit>($1);
    }
    ;

condition:
        col op expr
    {
        $$ = std::make_shared<BinaryExpr>($1, $2, $3);
    }
    ;

optWhereClause:
        /* epsilon */
    {
        $$ = {};
    }
    |   WHERE whereClause
    {
        $$ = $2;
    }
    ;

whereClause:
        condition 
    {
        $$ = std::vector<std::shared_ptr<BinaryExpr>>{$1};
    }
    |   whereClause AND condition
    {
        $$.push_back($3);
    }
    ;

col:
        tbName '.' colName
    {
        $$ = std::make_shared<Col>($1, $3);
    }
    |   colName
    {
        $$ = std::make_shared<Col>("", $1);
    }
    ;

colList:
        col
    {
        $$ = std::vector<std::shared_ptr<Col>>{$1};
    }
    |   colList ',' col
    {
        $$.push_back($3);
    }
    ;

op:
        '='
    {
        $$ = SV_OP_EQ;
    }
    |   '<'
    {
        $$ = SV_OP_LT;
    }
    |   '>'
    {
        $$ = SV_OP_GT;
    }
    |   NEQ
    {
        $$ = SV_OP_NE;
    }
    |   LEQ
    {
        $$ = SV_OP_LE;
    }
    |   GEQ
    {
        $$ = SV_OP_GE;
    }
    ;

expr:
        value
    {
        $$ = std::static_pointer_cast<Expr>($1);
    }
    |   col
    {
        $$ = std::static_pointer_cast<Expr>($1);
    }
    ;

setClauses:
        setClause
    {
        $$ = std::vector<std::shared_ptr<SetClause>>{$1};
    }
    |   setClauses ',' setClause
    {
        $$.push_back($3);
    }
    ;

setClause:
        colName '=' value
    {
        $$ = std::make_shared<SetClause>($1, $3);
    }
    ;

selector:
        '*'
    {
        $$ = {};
    }
    |   colList
    {
        $$ = std::move($1);
    }
    ;

tableList:
        tbName
    {
        $$ = std::vector<std::string>{$1};
    }
    |   tableList ',' tbName
    {
        $$.push_back($3);
    }
    |   tableList JOIN tbName
    {
        $$.push_back($3);
    }
    ;

opt_order_clause:
    ORDER BY order_clause      
    { 
        $$ = $3; 
    }
    |   /* epsilon */
    {
        $$ = nullptr;
    }
    ;

order_clause:
      col  opt_asc_desc 
    { 
        $$ = std::make_shared<OrderBy>($1, $2);
    }
    ;   

opt_asc_desc:
    ASC          { $$ = OrderBy_ASC;     }
    |  DESC      { $$ = OrderBy_DESC;    }
    |  /* epsilon */ { $$ = OrderBy_DEFAULT; }
    ;    

tbName:
    IDENTIFIER { $$ = std::move($1); }
    ;

colName:
    IDENTIFIER { $$ = std::move($1); }
    ;
%%
