    /**
     * @file lex.l
     * @brief 将 SQL 文本转换为 Bison token 的 Flex 词法规则。
     *
     * 关键字不区分大小写；空白和注释会被忽略。token 位置采用从 1 开始计数的
     * UTF-8 字节列号，用于生成解析诊断。字面量转换失败时，先把准确原因记录到
     * 本次调用的 ParseContext，再向 Bison 返回 INVALID。
     *
     * Copyright (c) 2023-2026 Renmin University of China
     * SPDX-License-Identifier: MulanPSL-2.0
     */

    /* 关键字不区分大小写。 */
%option caseless
    /* 扫描完整个输入缓冲区后直接结束，不调用 yywrap()。 */
%option noyywrap
    /* 词法规则不需要把字符放回输入流。 */
%option nounput
    /* 词法规则不直接读取单个字符。 */
%option noinput
    /* 让 Flex 同时接收 Bison 的语义值和源码位置。 */
%option bison-bridge
%option bison-locations

%{
#include "parser_internal.h"
#include "yacc.tab.h"

#define YY_DECL int yylex(YYSTYPE *yylval_param, YYLTYPE *yylloc_param, \
                         rucbase::parser::ParseContext *context)

/** @brief 为每个 token 或被忽略的词素更新源码位置。 */
#define YY_USER_ACTION \
    yylloc->first_line = yylloc->last_line; \
    yylloc->first_column = yylloc->last_column; \
    for (int i = 0; yytext[i] != '\0'; i++) { \
        if(yytext[i] == '\n') { \
            yylloc->last_line++; \
            yylloc->last_column = 1; \
        } else { \
            yylloc->last_column++; \
        } \
    }

%}

alpha [a-zA-Z]
digit [0-9]
white_space [ \t]+
new_line "\r"|"\n"|"\r\n"
sign "+"|"-"
identifier {alpha}(_|{alpha}|{digit})*
value_int {sign}?{digit}+
value_float {sign}?{digit}+\.({digit}+)?
value_string '([^']|'')*'
single_op ";"|"("|")"|","|"*"|"="|">"|"<"|"."

%x STATE_COMMENT

%%
    /* 块注释 */
"/*" {
    context->block_comment_line = yylloc->first_line;
    context->block_comment_column = yylloc->first_column;
    BEGIN(STATE_COMMENT);
}
<STATE_COMMENT>"*/" { BEGIN(INITIAL); }
<STATE_COMMENT>[^*] { /* 忽略注释正文。 */ }
<STATE_COMMENT>\* { /* 忽略不属于结束标记的星号。 */ }
<STATE_COMMENT><<EOF>> {
    context->RecordError(context->block_comment_line, context->block_comment_column,
                         "unterminated block comment");
    BEGIN(INITIAL);
    return INVALID;
}
    /* 单行注释 */
"--".* { /* 忽略单行注释。 */ }
    /* 空白与换行 */
{white_space} { /* 忽略空白。 */ }
{new_line} { /* 忽略换行。 */ }
    /* 关键字 */
"SHOW" { return SHOW; }
"DATABASE" { return DATABASE; }
"BEGIN" { return TXN_BEGIN; }
"COMMIT" { return TXN_COMMIT; }
"ABORT" { return TXN_ABORT; }
"ROLLBACK" { return TXN_ROLLBACK; }
"TABLES" { return TABLES; }
"CREATE" { return CREATE; }
"TABLE" { return TABLE; }
"DROP" { return DROP; }
"DESC" { return DESC; }
"INSERT" { return INSERT; }
"INTO" { return INTO; }
"VALUES" { return VALUES; }
"DELETE" { return DELETE; }
"FROM" { return FROM; }
"WHERE" { return WHERE; }
"UPDATE" { return UPDATE; }
"SET" { return SET; }
"SELECT" { return SELECT; }
"INT" { return INT; }
"CHAR" { return CHAR; }
"FLOAT" { return FLOAT; }
"INDEX" { return INDEX; }
"AND" { return AND; }
"JOIN" {return JOIN;}
"HELP" { return HELP; }
"ORDER" { return ORDER; }
"BY" {  return BY;  }
"ASC" { return ASC; }
    /* 运算符 */
">=" { return GEQ; }
"<=" { return LEQ; }
"!=" { return NEQ; }
"<>" { return NEQ; }
{single_op} { return yytext[0]; }
    /* 标识符 */
{identifier} {
    yylval->sv_str = yytext;
    return IDENTIFIER;
}
    /* 字面量 */
{value_int} {
    if (!rucbase::parser::ParseIntegerLiteral(yytext, yyleng, &yylval->sv_int)) {
        context->RecordError(yylloc->first_line, yylloc->first_column,
                             "integer literal is out of range");
        return INVALID;
    }
    return VALUE_INT;
}
{value_float} {
    if (!rucbase::parser::ParseFloatLiteral(yytext, yyleng, &yylval->sv_float)) {
        context->RecordError(yylloc->first_line, yylloc->first_column,
                             "floating-point literal is out of range");
        return INVALID;
    }
    return VALUE_FLOAT;
}
{value_string} {
    yylval->sv_str = rucbase::parser::DecodeStringLiteral(yytext, yyleng);
    return VALUE_STRING;
}
\'([^']|\'\')* {
    context->RecordError(yylloc->first_line, yylloc->first_column,
                         "unterminated string literal");
    return INVALID;
}
    /* 输入结束 */
<<EOF>> { return 0; }
    /* 非法字符 */
. {
    context->RecordError(yylloc->first_line, yylloc->first_column,
                         "unexpected character");
    return INVALID;
}
%%
