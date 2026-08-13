// Copyright (c) 2023-2027 Renmin University of China
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
    ParseResult result = Parse("show database;");
    ASSERT_TRUE(result.ok());
    EXPECT_NE(std::dynamic_pointer_cast<ast::ShowDatabaseStmt>(result.statement), nullptr);

    result = Parse("show tables;");
    ASSERT_TRUE(result.ok());
    EXPECT_NE(std::dynamic_pointer_cast<ast::ShowTablesStmt>(result.statement), nullptr);

    result = Parse("help;");
    ASSERT_TRUE(result.ok());
    EXPECT_NE(std::dynamic_pointer_cast<ast::HelpStmt>(result.statement), nullptr);

    result = Parse("create table tb (a int, b float, c char(4));");
    ASSERT_TRUE(result.ok());
    const auto create = std::dynamic_pointer_cast<ast::CreateTableStmt>(result.statement);
    ASSERT_NE(create, nullptr);
    EXPECT_EQ(create->tab_name, "tb");
    ASSERT_EQ(create->fields.size(), 3U);
    EXPECT_EQ(create->fields[0].col_name, "a");
    EXPECT_EQ(create->fields[0].type_len.type, ast::DataType::Int);
    EXPECT_EQ(create->fields[1].type_len.type, ast::DataType::Float);
    EXPECT_EQ(create->fields[2].type_len.type, ast::DataType::String);
    EXPECT_EQ(create->fields[2].type_len.declared_len, 4);
}

TEST(ParserTest, ParsesCreateIndexUniqueFlag) {
    ParseResult result = Parse("create index grade (id);");
    ASSERT_TRUE(result.ok());
    auto index = std::dynamic_pointer_cast<ast::CreateIndexStmt>(result.statement);
    ASSERT_NE(index, nullptr);
    EXPECT_EQ(index->tab_name, "grade");
    ASSERT_EQ(index->col_names.size(), 1U);
    EXPECT_EQ(index->col_names[0], "id");
    EXPECT_FALSE(index->unique);

    result = Parse("create unique index grade (id, name);");
    ASSERT_TRUE(result.ok());
    index = std::dynamic_pointer_cast<ast::CreateIndexStmt>(result.statement);
    ASSERT_NE(index, nullptr);
    EXPECT_EQ(index->tab_name, "grade");
    ASSERT_EQ(index->col_names.size(), 2U);
    EXPECT_TRUE(index->unique);
}

TEST(ParserTest, ParsesDmlAndDecodesEscapedQuotes) {
    ParseResult result = Parse("insert into tb values (1, 3.14, 'Tom''s book');");
    ASSERT_TRUE(result.ok());
    const auto insert = std::dynamic_pointer_cast<ast::InsertStmt>(result.statement);
    ASSERT_NE(insert, nullptr);
    ASSERT_EQ(insert->values.size(), 3U);
    const auto text = std::dynamic_pointer_cast<ast::StringLit>(insert->values[2]);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->val, "Tom's book");

    result = Parse(
        "select x.a, y.b from x join y "
        "where x.a = y.b and y.name = 'Tom''s book' order by x.a desc;");
    ASSERT_TRUE(result.ok());
    const auto select = std::dynamic_pointer_cast<ast::SelectStmt>(result.statement);
    ASSERT_NE(select, nullptr);
    ASSERT_EQ(select->select_items.size(), 2U);
    const auto join = std::dynamic_pointer_cast<ast::JoinNode>(select->from);
    ASSERT_NE(join, nullptr);
    EXPECT_EQ(join->type, ast::JoinType::Inner);
    const auto left_table = std::dynamic_pointer_cast<ast::TableRef>(join->left);
    const auto right_table = std::dynamic_pointer_cast<ast::TableRef>(join->right);
    ASSERT_NE(left_table, nullptr);
    ASSERT_NE(right_table, nullptr);
    EXPECT_EQ(std::get<std::string>(left_table->source), "x");
    EXPECT_EQ(std::get<std::string>(right_table->source), "y");
    ASSERT_NE(std::dynamic_pointer_cast<ast::LogicalExpr>(select->where), nullptr);
    ASSERT_EQ(select->order_by.size(), 1U);
    const auto order_column = std::dynamic_pointer_cast<ast::Col>(select->order_by[0]->expression);
    ASSERT_NE(order_column, nullptr);
    EXPECT_EQ(order_column->tab_name, "x");
    EXPECT_EQ(order_column->col_name, "a");
    EXPECT_EQ(select->order_by[0]->direction, ast::OrderByDir::Desc);
}

TEST(ParserTest, SupportsBothNotEqualSpellings) {
    for (const char* spelling : {"!=", "<>"}) {
        const ParseResult result = Parse("select * from tb where a " + std::string(spelling) + " 1;");
        ASSERT_TRUE(result.ok());
        const auto select = std::dynamic_pointer_cast<ast::SelectStmt>(result.statement);
        ASSERT_NE(select, nullptr);
        const auto comparison = std::dynamic_pointer_cast<ast::BinaryExpr>(select->where);
        ASSERT_NE(comparison, nullptr);
        EXPECT_EQ(comparison->op, ast::CompOp::Ne);
    }
}

TEST(ParserTest, ParsesQualifiedSetTargetAndExpressionRightHandSide) {
    const ParseResult result = Parse("update t set t.total = t.score, t.rank = abs(t.score);");
    ASSERT_TRUE(result.ok());
    const auto update = std::dynamic_pointer_cast<ast::UpdateStmt>(result.statement);
    ASSERT_NE(update, nullptr);
    ASSERT_EQ(update->set_clauses.size(), 2U);

    ASSERT_NE(update->set_clauses[0]->target_column, nullptr);
    EXPECT_EQ(update->set_clauses[0]->target_column->tab_name, "t");
    EXPECT_EQ(update->set_clauses[0]->target_column->col_name, "total");
    const auto source_column = std::dynamic_pointer_cast<ast::Col>(update->set_clauses[0]->assigned_expr);
    ASSERT_NE(source_column, nullptr);
    EXPECT_EQ(source_column->tab_name, "t");
    EXPECT_EQ(source_column->col_name, "score");

    const auto abs = std::dynamic_pointer_cast<ast::FunctionCallExpr>(update->set_clauses[1]->assigned_expr);
    ASSERT_NE(abs, nullptr);
    EXPECT_EQ(abs->name, "abs");
    ASSERT_EQ(abs->arguments.size(), 1U);
    const auto argument = std::dynamic_pointer_cast<ast::Col>(abs->arguments[0]);
    ASSERT_NE(argument, nullptr);
    EXPECT_EQ(argument->tab_name, "t");
    EXPECT_EQ(argument->col_name, "score");
}

TEST(ParserTest, PreservesCommaAndExplicitJoinKinds) {
    ParseResult result = Parse("select * from x, y;");
    ASSERT_TRUE(result.ok());
    auto select = std::dynamic_pointer_cast<ast::SelectStmt>(result.statement);
    ASSERT_NE(select, nullptr);
    auto join = std::dynamic_pointer_cast<ast::JoinNode>(select->from);
    ASSERT_NE(join, nullptr);
    EXPECT_EQ(join->type, ast::JoinType::Cross);

    result = Parse("select * from x join y;");
    ASSERT_TRUE(result.ok());
    select = std::dynamic_pointer_cast<ast::SelectStmt>(result.statement);
    ASSERT_NE(select, nullptr);
    join = std::dynamic_pointer_cast<ast::JoinNode>(select->from);
    ASSERT_NE(join, nullptr);
    EXPECT_EQ(join->type, ast::JoinType::Inner);
}

TEST(ParserTest, BuildsRecursiveLeftDeepJoinTree) {
    const ParseResult result = Parse("select * from x join y, z;");
    ASSERT_TRUE(result.ok());
    const auto select = std::dynamic_pointer_cast<ast::SelectStmt>(result.statement);
    ASSERT_NE(select, nullptr);

    const auto root = std::dynamic_pointer_cast<ast::JoinNode>(select->from);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->type, ast::JoinType::Cross);
    const auto left_join = std::dynamic_pointer_cast<ast::JoinNode>(root->left);
    const auto right_table = std::dynamic_pointer_cast<ast::TableRef>(root->right);
    ASSERT_NE(left_join, nullptr);
    ASSERT_NE(right_table, nullptr);
    EXPECT_EQ(left_join->type, ast::JoinType::Inner);
    EXPECT_EQ(std::get<std::string>(right_table->source), "z");
}

TEST(ParserTest, OptionalClausesDoNotLeakAcrossCalls) {
    ParseResult result = Parse("select * from tb where a = 1 order by a;");
    ASSERT_TRUE(result.ok());

    for (int iteration = 0; iteration < 3; ++iteration) {
        result = Parse("select * from tb;");
        ASSERT_TRUE(result.ok());
        const auto select = std::dynamic_pointer_cast<ast::SelectStmt>(result.statement);
        ASSERT_NE(select, nullptr);
        ASSERT_EQ(select->select_items.size(), 1U);
        EXPECT_NE(std::dynamic_pointer_cast<ast::StarExpr>(select->select_items[0]), nullptr);
        EXPECT_EQ(select->where, nullptr);
        EXPECT_TRUE(select->order_by.empty());
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
