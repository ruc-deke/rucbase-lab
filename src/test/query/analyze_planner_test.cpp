// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "analyze/analyze.h"
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
    QueryPlanningTest() : analyze_(&sm_manager_), planner_(&sm_manager_), optimizer_(&sm_manager_, &planner_) {
        AddTestTables(sm_manager_);
    }

    std::shared_ptr<DDLPlan> PlanCreateTable(const std::string& sql) {
        auto parsed = rucbase::parser::Parse(sql);
        EXPECT_TRUE(parsed.ok());
        if (!parsed.ok()) {
            return nullptr;
        }
        auto query = analyze_.do_analyze(std::move(parsed.statement));
        return std::dynamic_pointer_cast<DDLPlan>(planner_.do_planner(std::move(query), nullptr));
    }

    SmManager sm_manager_{nullptr, nullptr, nullptr, nullptr};
    Analyze analyze_;
    Planner planner_;
    Optimizer optimizer_;
};

TEST_F(QueryPlanningTest, PlansShowDatabaseAsUtility) {
    auto parsed = rucbase::parser::Parse("show database;");
    ASSERT_TRUE(parsed.ok());

    auto query = analyze_.do_analyze(std::move(parsed.statement));
    auto utility = std::dynamic_pointer_cast<OtherPlan>(optimizer_.plan_query(query, nullptr));
    ASSERT_NE(utility, nullptr);
    EXPECT_EQ(utility->tag, T_ShowDatabase);
}

TEST_F(QueryPlanningTest, RejectsQualifiedColumnOutsideFromScope) {
    auto parsed = rucbase::parser::Parse("select b.id from a;");
    ASSERT_TRUE(parsed.ok());

    EXPECT_THROW(analyze_.do_analyze(parsed.statement), ColumnNotFoundError);
}

TEST_F(QueryPlanningTest, RejectsQualifiedPredicateOutsideFromScope) {
    auto parsed = rucbase::parser::Parse("select * from a where b.id = 1;");
    ASSERT_TRUE(parsed.ok());

    EXPECT_THROW(analyze_.do_analyze(parsed.statement), ColumnNotFoundError);
}

TEST_F(QueryPlanningTest, RejectsLiteralTypeBeforeEncodingRawValue) {
    auto parsed = rucbase::parser::Parse("select * from a where a.text = 1;");
    ASSERT_TRUE(parsed.ok());

    EXPECT_THROW(analyze_.do_analyze(parsed.statement), IncompatibleTypeError);
}

TEST_F(QueryPlanningTest, AssignsSameTableColumnPredicateToThatTable) {
    auto parsed = rucbase::parser::Parse("select * from a join b where b.x = b.y;");
    ASSERT_TRUE(parsed.ok());
    auto query = analyze_.do_analyze(parsed.statement);

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
    EXPECT_THROW(sm_manager_.create_table(
                     "too_wide",
                     {{.name = "left", .type = TYPE_STRING, .len = 300},
                      {.name = "right", .type = TYPE_STRING, .len = 300}},
                     nullptr),
                 InvalidRecordSizeError);
}

TEST_F(QueryPlanningTest, BindsQualifiedOrderByColumnWithinFromScope) {
    auto parsed = rucbase::parser::Parse("select a.text from a join b order by a.id desc;");
    ASSERT_TRUE(parsed.ok());
    auto query = analyze_.do_analyze(std::move(parsed.statement));
    ASSERT_TRUE(query->order_col.has_value());
    EXPECT_EQ(query->order_col->tab_name, "a");
    EXPECT_EQ(query->order_col->col_name, "id");
    EXPECT_TRUE(query->order_desc);

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
    EXPECT_THROW(analyze_.do_analyze(std::move(outside_scope.statement)), ColumnNotFoundError);

    auto missing = rucbase::parser::Parse("select a.id from a order by missing;");
    ASSERT_TRUE(missing.ok());
    EXPECT_THROW(analyze_.do_analyze(std::move(missing.statement)), ColumnNotFoundError);

    auto ambiguous = rucbase::parser::Parse("select a.id from a join b order by id;");
    ASSERT_TRUE(ambiguous.ok());
    EXPECT_THROW(analyze_.do_analyze(std::move(ambiguous.statement)), AmbiguousColumnError);
}

}  // namespace
