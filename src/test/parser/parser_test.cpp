// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "parser/parser.h"

#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

using rucbase::parser::Parse;
using rucbase::parser::ParseResult;

TEST(ParserTest, ParsesUtilityAndDdlStatements) {
    ParseResult result = Parse("show tables;");
    ASSERT_TRUE(result.ok());
    EXPECT_NE(std::dynamic_pointer_cast<ast::ShowTables>(result.statement), nullptr);

    result = Parse("help;");
    ASSERT_TRUE(result.ok());
    EXPECT_NE(std::dynamic_pointer_cast<ast::Help>(result.statement), nullptr);

    result = Parse("create table tb (a int, b float, c char(4));");
    ASSERT_TRUE(result.ok());
    const auto create = std::dynamic_pointer_cast<ast::CreateTable>(result.statement);
    ASSERT_NE(create, nullptr);
    EXPECT_EQ(create->tab_name, "tb");
    ASSERT_EQ(create->fields.size(), 3U);
    EXPECT_EQ(create->fields[0].col_name, "a");
    EXPECT_EQ(create->fields[0].type_len.type, ast::SV_TYPE_INT);
    EXPECT_EQ(create->fields[1].type_len.type, ast::SV_TYPE_FLOAT);
    EXPECT_EQ(create->fields[2].type_len.type, ast::SV_TYPE_STRING);
    EXPECT_EQ(create->fields[2].type_len.len, 4);
}

TEST(ParserTest, ParsesDmlAndDecodesEscapedQuotes) {
    ParseResult result = Parse("insert into tb values (1, 3.14, 'Tom''s book');");
    ASSERT_TRUE(result.ok());
    const auto insert = std::dynamic_pointer_cast<ast::InsertStmt>(result.statement);
    ASSERT_NE(insert, nullptr);
    ASSERT_EQ(insert->vals.size(), 3U);
    const auto text = std::dynamic_pointer_cast<ast::StringLit>(insert->vals[2]);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->val, "Tom's book");

    result = Parse(
        "select x.a, y.b from x join y "
        "where x.a = y.b and y.name = 'Tom''s book' order by x.a desc;");
    ASSERT_TRUE(result.ok());
    const auto select = std::dynamic_pointer_cast<ast::SelectStmt>(result.statement);
    ASSERT_NE(select, nullptr);
    ASSERT_EQ(select->cols.size(), 2U);
    ASSERT_EQ(select->tabs, (std::vector<std::string>{"x", "y"}));
    ASSERT_EQ(select->conds.size(), 2U);
    ASSERT_NE(select->order, nullptr);
    EXPECT_EQ(select->order->column->tab_name, "x");
    EXPECT_EQ(select->order->column->col_name, "a");
    EXPECT_EQ(select->order->direction, ast::OrderBy_DESC);
}

TEST(ParserTest, SupportsBothNotEqualSpellings) {
    for (const char* spelling : {"!=", "<>"}) {
        const ParseResult result = Parse("select * from tb where a " + std::string(spelling) + " 1;");
        ASSERT_TRUE(result.ok());
        const auto select = std::dynamic_pointer_cast<ast::SelectStmt>(result.statement);
        ASSERT_NE(select, nullptr);
        ASSERT_EQ(select->conds.size(), 1U);
        EXPECT_EQ(select->conds.front()->op, ast::SV_OP_NE);
    }
}

TEST(ParserTest, OptionalClausesDoNotLeakAcrossCalls) {
    ParseResult result = Parse("select * from tb where a = 1 order by a;");
    ASSERT_TRUE(result.ok());

    for (int iteration = 0; iteration < 3; ++iteration) {
        result = Parse("select * from tb;");
        ASSERT_TRUE(result.ok());
        const auto select = std::dynamic_pointer_cast<ast::SelectStmt>(result.statement);
        ASSERT_NE(select, nullptr);
        EXPECT_TRUE(select->conds.empty());
        EXPECT_EQ(select->order, nullptr);
    }
}

TEST(ParserTest, RequiresExactlyOneTerminatedStatement) {
    EXPECT_TRUE(Parse("show tables; -- trailing comment").ok());
    EXPECT_TRUE(Parse("show tables; /* trailing comment */").ok());
    EXPECT_FALSE(Parse("").ok());
    EXPECT_FALSE(Parse("show tables").ok());
    EXPECT_FALSE(Parse("show tables; garbage").ok());
    EXPECT_FALSE(Parse("show tables; show tables;").ok());

    // EXIT is a client command rather than part of the SQL grammar.
    EXPECT_FALSE(Parse("exit;").ok());
}

TEST(ParserTest, ReportsPreciseSyntaxErrors) {
    const std::string sql = "show table;";
    const ParseResult result = Parse(sql);
    ASSERT_FALSE(result.ok());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->line, 1);
    EXPECT_EQ(result.error->column, 6);
    EXPECT_NE(result.error->message.find("unexpected TABLE"), std::string::npos);

    const std::string diagnostic = rucbase::parser::FormatError(sql, *result.error);
    EXPECT_NE(diagnostic.find("Parser Error at line 1 column 6"), std::string::npos);
    EXPECT_NE(diagnostic.find("\nshow table;\n     ^\n"), std::string::npos);
}

TEST(ParserTest, RejectsMalformedLexemes) {
    ParseResult result = Parse("insert into tb values (999999999999999999999999);");
    ASSERT_FALSE(result.ok());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->message, "integer literal is out of range");

    const std::string oversized_float = "insert into tb values (" + std::string(400, '9') + ".0);";
    result = Parse(oversized_float);
    ASSERT_FALSE(result.ok());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->message, "floating-point literal is out of range");

    result = Parse("insert into tb values ('unfinished);");
    ASSERT_FALSE(result.ok());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->message, "unterminated string literal");

    result = Parse("show tables; /* unfinished");
    ASSERT_FALSE(result.ok());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->message, "unterminated block comment");

    std::string embedded_nul = "show tables;";
    embedded_nul.push_back('\0');
    embedded_nul += "garbage";
    result = Parse(embedded_nul);
    ASSERT_FALSE(result.ok());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->message, "NUL byte is not allowed in SQL");

    const std::string full_width_semicolon = "show tables；;";
    result = Parse(full_width_semicolon);
    ASSERT_FALSE(result.ok());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->line, 1);
    EXPECT_EQ(result.error->column, 12);
    EXPECT_EQ(result.error->message, "unexpected character");
    EXPECT_NE(
        rucbase::parser::FormatError(full_width_semicolon, *result.error).find("\nshow tables；;\n           ^\n"),
        std::string::npos);
}

TEST(ParserTest, TracksLocationsAcrossLines) {
    const ParseResult result = Parse("select *\nfrom tb\nwhere a = ;");
    ASSERT_FALSE(result.ok());
    ASSERT_TRUE(result.error.has_value());
    EXPECT_EQ(result.error->line, 3);
    EXPECT_EQ(result.error->column, 11);
}

TEST(ParserTest, ConcurrentCallsAreSafe) {
    std::atomic<bool> success{true};
    std::vector<std::thread> threads;
    threads.reserve(8);
    for (int thread_index = 0; thread_index < 8; ++thread_index) {
        threads.emplace_back([&success] {
            for (int iteration = 0; iteration < 50; ++iteration) {
                if (!Parse("select a from tb where a = 1;").ok()) {
                    success.store(false);
                    return;
                }
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    EXPECT_TRUE(success.load());
}

}  // namespace
