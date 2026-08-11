// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>

#include "analyze/analyzer.h"
#include "optimizer/optimizer.h"
#include "optimizer/planner.h"
#include "parser/parser.h"

namespace {

void AddTestTables(SmManager& sm_manager) {
    TabMeta a;
    a.name = "a";
    a.cols = {{.tab_name = "a", .name = "id", .type = TYPE_INT, .len = 4, .offset = 0, .index = false},
              {.tab_name = "a", .name = "text", .type = TYPE_STRING, .len = 8, .offset = 4, .index = false}};
    sm_manager.db_.SetTabMeta(a.name, a);

    TabMeta b;
    b.name = "b";
    b.cols = {{.tab_name = "b", .name = "id", .type = TYPE_INT, .len = 4, .offset = 0, .index = false},
              {.tab_name = "b", .name = "x", .type = TYPE_INT, .len = 4, .offset = 4, .index = false},
              {.tab_name = "b", .name = "y", .type = TYPE_INT, .len = 4, .offset = 8, .index = false}};
    sm_manager.db_.SetTabMeta(b.name, b);

    TabMeta c;
    c.name = "c";
    c.cols = {{.tab_name = "c", .name = "value", .type = TYPE_INT, .len = 4, .offset = 0, .index = false}};
    sm_manager.db_.SetTabMeta(c.name, c);
}

std::shared_ptr<ScanPlan> FindScan(const std::shared_ptr<Plan>& plan, const std::string& table) {
    if (auto scan = std::dynamic_pointer_cast<ScanPlan>(plan)) {
        return scan->tab_name_ == table ? scan : nullptr;
    }
    if (auto dml = std::dynamic_pointer_cast<DMLPlan>(plan)) {
        return FindScan(dml->subplan_, table);
    }
    if (auto projection = std::dynamic_pointer_cast<ProjectionPlan>(plan)) {
        return FindScan(projection->subplan_, table);
    }
    if (auto sort = std::dynamic_pointer_cast<SortPlan>(plan)) {
        return FindScan(sort->subplan_, table);
    }
    if (auto join = std::dynamic_pointer_cast<JoinPlan>(plan)) {
        auto scan = FindScan(join->left_, table);
        return scan != nullptr ? scan : FindScan(join->right_, table);
    }
    return nullptr;
}

class QueryPlanningTest : public ::testing::Test {
protected:
    QueryPlanningTest() : analyzer_(&sm_manager_), planner_(&sm_manager_), optimizer_(&sm_manager_, &planner_) {
        AddTestTables(sm_manager_);
    }

    std::shared_ptr<DDLPlan> PlanCreateTable(const std::string& sql) {
        auto parsed = rucbase::parser::Parse(sql);
        EXPECT_TRUE(parsed.ok());
        if (!parsed.ok()) {
            return nullptr;
        }
        auto query = analyzer_.analyze(parsed.statement);
        return std::dynamic_pointer_cast<DDLPlan>(planner_.do_planner(std::move(query), nullptr));
    }

    SmManager sm_manager_{nullptr, nullptr, nullptr, nullptr};
    Analyzer analyzer_;
    Planner planner_;
    Optimizer optimizer_;
};

TEST_F(QueryPlanningTest, PlansShowDatabaseAsUtility) {
    auto parsed = rucbase::parser::Parse("show database;");
    ASSERT_TRUE(parsed.ok());

    auto query = analyzer_.analyze(parsed.statement);
    auto utility = std::dynamic_pointer_cast<OtherPlan>(optimizer_.plan_query(query, nullptr));
    ASSERT_NE(utility, nullptr);
    EXPECT_EQ(utility->tag, T_ShowDatabase);
}

TEST_F(QueryPlanningTest, RejectsQualifiedColumnOutsideFromScope) {
    auto parsed = rucbase::parser::Parse("select b.id from a;");
    ASSERT_TRUE(parsed.ok());

    EXPECT_THROW(analyzer_.analyze(parsed.statement), ColumnNotFoundError);
}

TEST_F(QueryPlanningTest, RejectsQualifiedPredicateOutsideFromScope) {
    auto parsed = rucbase::parser::Parse("select * from a where b.id = 1;");
    ASSERT_TRUE(parsed.ok());

    EXPECT_THROW(analyzer_.analyze(parsed.statement), ColumnNotFoundError);
}

TEST_F(QueryPlanningTest, RejectsLiteralTypeBeforeEncodingRawValue) {
    auto parsed = rucbase::parser::Parse("select * from a where a.text = 1;");
    ASSERT_TRUE(parsed.ok());

    EXPECT_THROW(analyzer_.analyze(parsed.statement), IncompatibleTypeError);
}

TEST_F(QueryPlanningTest, PreservesParsedAstAndAnalyzedConditionsDuringPlanning) {
    auto parsed = rucbase::parser::Parse("select a.id from a where a.id = 1;");
    ASSERT_TRUE(parsed.ok());
    auto select_stmt = std::dynamic_pointer_cast<ast::SelectStmt>(parsed.statement);
    ASSERT_NE(select_stmt, nullptr);
    auto table_ref = std::dynamic_pointer_cast<ast::TableRef>(select_stmt->from);
    ASSERT_NE(table_ref, nullptr);
    const auto parsed_table = std::get<std::string>(table_ref->source);

    auto query = analyzer_.analyze(parsed.statement);
    ASSERT_EQ(query->bound_statement.get(), parsed.statement.get());
    EXPECT_EQ(parsed.statement->kind(), ast::StatementKind::Select);
    table_ref = std::dynamic_pointer_cast<ast::TableRef>(select_stmt->from);
    ASSERT_NE(table_ref, nullptr);
    EXPECT_EQ(std::get<std::string>(table_ref->source), parsed_table);
    ASSERT_EQ(query->bound_conds.size(), 1U);

    auto plan = planner_.do_planner(query, nullptr);
    ASSERT_NE(plan, nullptr);
    ASSERT_EQ(query->bound_conds.size(), 1U);
    EXPECT_EQ(query->bound_conds[0].lhs_col.tab_name, "a");
    EXPECT_TRUE(query->bound_conds[0].is_rhs_val);
}

TEST_F(QueryPlanningTest, AnalyzesAndEncodesInsertValues) {
    auto parsed = rucbase::parser::Parse("insert into a values (1, 'short');");
    ASSERT_TRUE(parsed.ok());

    auto query = analyzer_.analyze(parsed.statement);
    EXPECT_EQ(query->bound_target_table, "a");
    ASSERT_EQ(query->bound_values.size(), 2U);
    ASSERT_NE(query->bound_values[0].raw, nullptr);
    ASSERT_NE(query->bound_values[1].raw, nullptr);
    EXPECT_EQ(query->bound_values[0].raw->size, 4);
    EXPECT_EQ(query->bound_values[1].raw->size, 8);
}

TEST_F(QueryPlanningTest, PlansDmlFromBoundTargetInsteadOfMutableAst) {
    auto parsed = rucbase::parser::Parse("insert into a values (1, 'short');");
    ASSERT_TRUE(parsed.ok());
    auto query = analyzer_.analyze(parsed.statement);

    const auto insert = std::dynamic_pointer_cast<ast::InsertStmt>(parsed.statement);
    ASSERT_NE(insert, nullptr);
    insert->tab_name = "b";

    const auto plan = std::dynamic_pointer_cast<DMLPlan>(planner_.do_planner(query, nullptr));
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->tab_name_, "a");
}

TEST_F(QueryPlanningTest, RejectsInvalidInsertBeforePlanning) {
    auto missing_table = rucbase::parser::Parse("insert into missing values (1);");
    ASSERT_TRUE(missing_table.ok());
    EXPECT_THROW(analyzer_.analyze(missing_table.statement), TableNotFoundError);

    auto wrong_count = rucbase::parser::Parse("insert into a values (1);");
    ASSERT_TRUE(wrong_count.ok());
    EXPECT_THROW(analyzer_.analyze(wrong_count.statement), InvalidValueCountError);

    auto wrong_type = rucbase::parser::Parse("insert into a values ('one', 'short');");
    ASSERT_TRUE(wrong_type.ok());
    EXPECT_THROW(analyzer_.analyze(wrong_type.statement), IncompatibleTypeError);

    auto oversized_string = rucbase::parser::Parse("insert into a values (1, '123456789');");
    ASSERT_TRUE(oversized_string.ok());
    EXPECT_THROW(analyzer_.analyze(oversized_string.statement), StringOverflowError);
}

TEST_F(QueryPlanningTest, BindsUpdateTargetColumn) {
    auto parsed = rucbase::parser::Parse("update a set a.text = 'short' where id = 1;");
    ASSERT_TRUE(parsed.ok());

    auto query = analyzer_.analyze(parsed.statement);
    ASSERT_EQ(query->bound_set_clauses.size(), 1U);
    EXPECT_EQ(query->bound_set_clauses[0].lhs.tab_name, "a");
    EXPECT_EQ(query->bound_set_clauses[0].lhs.col_name, "text");
    ASSERT_NE(query->bound_set_clauses[0].rhs.raw, nullptr);
    EXPECT_EQ(query->bound_set_clauses[0].rhs.raw->size, 8);
}

TEST_F(QueryPlanningTest, RejectsUnimplementedSelectExtensionsExplicitly) {
    auto parsed = rucbase::parser::Parse("select a.id from a;");
    ASSERT_TRUE(parsed.ok());
    auto select = std::dynamic_pointer_cast<ast::SelectStmt>(parsed.statement);
    ASSERT_NE(select, nullptr);
    select->limit = ast::LimitClause{.count = 1, .offset = std::nullopt};

    EXPECT_THROW(analyzer_.analyze(parsed.statement), NotImplementedError);
}

TEST_F(QueryPlanningTest, RejectsUnimplementedUpdateSetExpressionExplicitly) {
    auto parsed = rucbase::parser::Parse("update a set a.id = abs(a.id);");
    ASSERT_TRUE(parsed.ok());

    EXPECT_THROW(analyzer_.analyze(parsed.statement), NotImplementedError);
}

TEST_F(QueryPlanningTest, AssignsSameTableColumnPredicateToThatTable) {
    auto parsed = rucbase::parser::Parse("select * from a join b where b.x = b.y;");
    ASSERT_TRUE(parsed.ok());
    auto query = analyzer_.analyze(parsed.statement);
    EXPECT_EQ(query->bound_tables, (std::vector<std::string>{"a", "b"}));

    auto plan = planner_.do_planner(std::move(query), nullptr);
    auto a_scan = FindScan(plan, "a");
    auto b_scan = FindScan(plan, "b");
    ASSERT_NE(a_scan, nullptr);
    ASSERT_NE(b_scan, nullptr);
    EXPECT_TRUE(a_scan->conds_.empty());
    ASSERT_EQ(b_scan->conds_.size(), 1U);
    EXPECT_EQ(b_scan->conds_[0].lhs_col.tab_name, "b");
    EXPECT_EQ(b_scan->conds_[0].rhs_col.tab_name, "b");
}

TEST_F(QueryPlanningTest, PreservesFromTableOrderDuringAnalysis) {
    auto parsed = rucbase::parser::Parse("select a.id from a join b, c;");
    ASSERT_TRUE(parsed.ok());
    auto query = analyzer_.analyze(parsed.statement);

    EXPECT_EQ(query->bound_tables, (std::vector<std::string>{"a", "b", "c"}));
}

TEST_F(QueryPlanningTest, RejectsNonPositiveCharLengthsBeforeFileCreation) {
    auto negative = PlanCreateTable("create table negative_len(c char(-1));");
    ASSERT_NE(negative, nullptr);
    EXPECT_THROW(sm_manager_.create_table(negative->tab_name_, negative->cols_, nullptr), InvalidColLengthError);

    auto zero = PlanCreateTable("create table zero_len(c char(0));");
    ASSERT_NE(zero, nullptr);
    EXPECT_THROW(sm_manager_.create_table(zero->tab_name_, zero->cols_, nullptr), InvalidColLengthError);
}

TEST_F(QueryPlanningTest, RejectsDuplicateColumnsBeforeFileCreation) {
    auto plan = PlanCreateTable("create table duplicate_cols(id int, id float);");
    ASSERT_NE(plan, nullptr);

    EXPECT_THROW(sm_manager_.create_table(plan->tab_name_, plan->cols_, nullptr), ColumnExistsError);
}

TEST_F(QueryPlanningTest, RejectsOtherInvalidRecordLayoutsBeforeFileCreation) {
    EXPECT_THROW(sm_manager_.create_table("empty_cols", {}, nullptr), InvalidRecordSizeError);
    EXPECT_THROW(sm_manager_.create_table("bad_int", {{.name = "id", .type = TYPE_INT, .len = 1}}, nullptr),
                 InvalidColLengthError);
    EXPECT_THROW(sm_manager_.create_table("bad_float", {{.name = "score", .type = TYPE_FLOAT, .len = 8}}, nullptr),
                 InvalidColLengthError);
    EXPECT_THROW(sm_manager_.create_table("too_wide",
                                          {{.name = "left", .type = TYPE_STRING, .len = 300},
                                           {.name = "right", .type = TYPE_STRING, .len = 300}},
                                          nullptr),
                 InvalidRecordSizeError);
}

TEST_F(QueryPlanningTest, BindsQualifiedOrderByColumnWithinFromScope) {
    auto parsed = rucbase::parser::Parse("select a.text from a join b order by a.id desc;");
    ASSERT_TRUE(parsed.ok());
    auto query = analyzer_.analyze(parsed.statement);
    ASSERT_TRUE(query->bound_order_col.has_value());
    EXPECT_EQ(query->bound_order_col->tab_name, "a");
    EXPECT_EQ(query->bound_order_col->col_name, "id");
    EXPECT_TRUE(query->bound_order_desc);

    auto dml = std::dynamic_pointer_cast<DMLPlan>(planner_.do_planner(std::move(query), nullptr));
    ASSERT_NE(dml, nullptr);
    auto projection = std::dynamic_pointer_cast<ProjectionPlan>(dml->subplan_);
    ASSERT_NE(projection, nullptr);
    auto sort = std::dynamic_pointer_cast<SortPlan>(projection->subplan_);
    ASSERT_NE(sort, nullptr);
    EXPECT_EQ(sort->sel_col_.tab_name, "a");
    EXPECT_EQ(sort->sel_col_.col_name, "id");
    EXPECT_TRUE(sort->is_desc_);
}

TEST_F(QueryPlanningTest, RejectsInvalidOrderByColumns) {
    auto outside_scope = rucbase::parser::Parse("select a.id from a order by b.id;");
    ASSERT_TRUE(outside_scope.ok());
    EXPECT_THROW(analyzer_.analyze(outside_scope.statement), ColumnNotFoundError);

    auto missing = rucbase::parser::Parse("select a.id from a order by missing;");
    ASSERT_TRUE(missing.ok());
    EXPECT_THROW(analyzer_.analyze(missing.statement), ColumnNotFoundError);

    auto ambiguous = rucbase::parser::Parse("select a.id from a join b order by id;");
    ASSERT_TRUE(ambiguous.ok());
    EXPECT_THROW(analyzer_.analyze(ambiguous.statement), AmbiguousColumnError);
}

}  // namespace
