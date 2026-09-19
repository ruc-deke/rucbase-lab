// Copyright (c) 2023-2027 Renmin University of China
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
    b.cols[1].index = true;
    b.cols[2].index = true;
    b.indexes = {
        {.tab_name = "b", .col_tot_len = 4, .col_num = 1, .cols = {b.cols[2]}},
        {.tab_name = "b", .col_tot_len = 8, .col_num = 2, .cols = {b.cols[1], b.cols[2]}},
    };
    sm_manager.db_.SetTabMeta(b.name, b);

    TabMeta c;
    c.name = "c";
    c.cols = {{.tab_name = "c", .name = "value", .type = TYPE_INT, .len = 4, .offset = 0, .index = false}};
    sm_manager.db_.SetTabMeta(c.name, c);

    TabMeta d;
    d.name = "d";
    d.cols = {{.tab_name = "d", .name = "value", .type = TYPE_INT, .len = 4, .offset = 0, .index = false}};
    sm_manager.db_.SetTabMeta(d.name, d);
}

ScanPlan* FindScan(Plan* plan, const std::string& table) {
    if (auto* scan = dynamic_cast<ScanPlan*>(plan)) {
        return scan->tab_name_ == table ? scan : nullptr;
    }
    if (auto* select = dynamic_cast<SelectPlan*>(plan)) {
        return FindScan(select->projection_.get(), table);
    }
    if (auto* update = dynamic_cast<UpdatePlan*>(plan)) {
        return FindScan(update->scan_.get(), table);
    }
    if (auto* delete_plan = dynamic_cast<DeletePlan*>(plan)) {
        return FindScan(delete_plan->scan_.get(), table);
    }
    if (auto* projection = dynamic_cast<ProjectionPlan*>(plan)) {
        return FindScan(projection->subplan_.get(), table);
    }
    if (auto* sort = dynamic_cast<SortPlan*>(plan)) {
        return FindScan(sort->subplan_.get(), table);
    }
    if (auto* join = dynamic_cast<JoinPlan*>(plan)) {
        auto* scan = FindScan(join->left_.get(), table);
        return scan != nullptr ? scan : FindScan(join->right_.get(), table);
    }
    return nullptr;
}

class QueryPlanningTest : public ::testing::Test {
protected:
    QueryPlanningTest() : analyzer_(&sm_manager_), planner_(&sm_manager_), optimizer_(&sm_manager_, &planner_) {
        AddTestTables(sm_manager_);
    }

    std::unique_ptr<Plan> PlanCreateTable(const std::string& sql) {
        auto parsed = rucbase::parser::Parse(sql);
        EXPECT_TRUE(parsed.ok());
        if (!parsed.ok()) {
            return nullptr;
        }
        auto query = analyzer_.analyze(parsed.statement);
        return planner_.do_planner(std::move(query), nullptr);
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
    auto plan = optimizer_.plan_query(query, nullptr);
    auto* utility = dynamic_cast<OtherPlan*>(plan.get());
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

    const auto plan = planner_.do_planner(query, nullptr);
    const auto* insert_plan = dynamic_cast<InsertPlan*>(plan.get());
    ASSERT_NE(insert_plan, nullptr);
    EXPECT_EQ(insert_plan->table_, "a");
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
    auto* a_scan = FindScan(plan.get(), "a");
    auto* b_scan = FindScan(plan.get(), "b");
    ASSERT_NE(a_scan, nullptr);
    ASSERT_NE(b_scan, nullptr);
    EXPECT_TRUE(a_scan->conds_.empty());
    ASSERT_EQ(b_scan->conds_.size(), 1U);
    EXPECT_EQ(b_scan->conds_[0].lhs_col.tab_name, "b");
    EXPECT_EQ(b_scan->conds_[0].rhs_col.tab_name, "b");
}

TEST_F(QueryPlanningTest, MatchesCompositeIndexRegardlessOfPredicateOrder) {
    auto first = rucbase::parser::Parse("select * from b where x = 1 and y = 2;");
    auto second = rucbase::parser::Parse("select * from b where y = 2 and x = 1;");
    ASSERT_TRUE(first.ok());
    ASSERT_TRUE(second.ok());

    auto first_plan = planner_.do_planner(analyzer_.analyze(first.statement), nullptr);
    auto second_plan = planner_.do_planner(analyzer_.analyze(second.statement), nullptr);
    auto* first_scan = FindScan(first_plan.get(), "b");
    auto* second_scan = FindScan(second_plan.get(), "b");
    ASSERT_NE(first_scan, nullptr);
    ASSERT_NE(second_scan, nullptr);
    EXPECT_EQ(first_scan->tag, T_IndexScan);
    EXPECT_EQ(second_scan->tag, T_IndexScan);
    EXPECT_EQ(first_scan->index_col_names_, (std::vector<std::string>{"x", "y"}));
    EXPECT_EQ(second_scan->index_col_names_, (std::vector<std::string>{"x", "y"}));
}

TEST_F(QueryPlanningTest, AllowsResidualPredicateWithIndexScan) {
    auto parsed = rucbase::parser::Parse("select * from b where id = 3 and y = 2 and x = 1;");
    ASSERT_TRUE(parsed.ok());

    auto plan = planner_.do_planner(analyzer_.analyze(parsed.statement), nullptr);
    auto* scan = FindScan(plan.get(), "b");
    ASSERT_NE(scan, nullptr);
    EXPECT_EQ(scan->tag, T_IndexScan);
    EXPECT_EQ(scan->index_col_names_, (std::vector<std::string>{"x", "y"}));
    EXPECT_EQ(scan->conds_.size(), 3U);
}

TEST_F(QueryPlanningTest, RequiresEveryCompositeIndexColumn) {
    auto parsed = rucbase::parser::Parse("select * from b where x = 1;");
    ASSERT_TRUE(parsed.ok());

    auto plan = planner_.do_planner(analyzer_.analyze(parsed.statement), nullptr);
    auto* scan = FindScan(plan.get(), "b");
    ASSERT_NE(scan, nullptr);
    EXPECT_EQ(scan->tag, T_SeqScan);
    EXPECT_TRUE(scan->index_col_names_.empty());
}

TEST_F(QueryPlanningTest, DoesNotChooseIndexForUnsupportedRangeScan) {
    auto parsed = rucbase::parser::Parse("select * from b where y > 1;");
    ASSERT_TRUE(parsed.ok());

    auto plan = planner_.do_planner(analyzer_.analyze(parsed.statement), nullptr);
    auto* scan = FindScan(plan.get(), "b");
    ASSERT_NE(scan, nullptr);
    EXPECT_EQ(scan->tag, T_SeqScan);
    EXPECT_TRUE(scan->index_col_names_.empty());
}

TEST_F(QueryPlanningTest, UsesSameIndexRuleForDelete) {
    auto parsed = rucbase::parser::Parse("delete from b where y = 2;");
    ASSERT_TRUE(parsed.ok());

    auto plan = planner_.do_planner(analyzer_.analyze(parsed.statement), nullptr);
    auto* scan = FindScan(plan.get(), "b");
    ASSERT_NE(scan, nullptr);
    EXPECT_EQ(scan->tag, T_IndexScan);
    EXPECT_EQ(scan->index_col_names_, (std::vector<std::string>{"y"}));
}

TEST_F(QueryPlanningTest, AttachesEveryPredicateBetweenJoinedTables) {
    auto parsed = rucbase::parser::Parse("select * from a join b where a.id = b.id and a.id = b.id;");
    ASSERT_TRUE(parsed.ok());

    auto query = analyzer_.analyze(parsed.statement);
    auto plan = planner_.do_planner(std::move(query), nullptr);
    auto* select = dynamic_cast<SelectPlan*>(plan.get());
    ASSERT_NE(select, nullptr);
    auto* join = dynamic_cast<JoinPlan*>(select->projection_->subplan_.get());
    ASSERT_NE(join, nullptr);
    EXPECT_EQ(join->conds_.size(), 2U);
}

TEST_F(QueryPlanningTest, RejectsJoinPredicateThatCannotBeAttached) {
    auto parsed = rucbase::parser::Parse("select * from a join b where a.id = b.id and a.id = b.id;");
    ASSERT_TRUE(parsed.ok());

    auto query = analyzer_.analyze(parsed.statement);
    ASSERT_EQ(query->bound_conds.size(), 2U);
    query->bound_conds[1].rhs_col.tab_name = "missing";

    EXPECT_THROW(planner_.do_planner(std::move(query), nullptr), InternalError);
}

TEST_F(QueryPlanningTest, BuildsConnectedMultiTableJoin) {
    auto parsed = rucbase::parser::Parse("select * from a join b, c where a.id = b.id and b.id = c.value;");
    ASSERT_TRUE(parsed.ok());

    auto plan = planner_.do_planner(analyzer_.analyze(parsed.statement), nullptr);
    EXPECT_NE(FindScan(plan.get(), "a"), nullptr);
    EXPECT_NE(FindScan(plan.get(), "b"), nullptr);
    EXPECT_NE(FindScan(plan.get(), "c"), nullptr);
}

TEST_F(QueryPlanningTest, BuildsJoinForDisconnectedTableGroups) {
    auto parsed = rucbase::parser::Parse("select * from a join b, c, d where a.id = b.id and c.value = d.value;");
    ASSERT_TRUE(parsed.ok());

    auto plan = planner_.do_planner(analyzer_.analyze(parsed.statement), nullptr);
    EXPECT_NE(FindScan(plan.get(), "a"), nullptr);
    EXPECT_NE(FindScan(plan.get(), "b"), nullptr);
    EXPECT_NE(FindScan(plan.get(), "c"), nullptr);
    EXPECT_NE(FindScan(plan.get(), "d"), nullptr);
}

TEST_F(QueryPlanningTest, PlansSetKnobAsUtility) {
    auto parsed = rucbase::parser::Parse("set ENABLE_SORTMERGE = True;");
    ASSERT_TRUE(parsed.ok());

    auto plan = optimizer_.plan_query(analyzer_.analyze(parsed.statement), nullptr);
    auto* set_knob = dynamic_cast<SetKnobPlan*>(plan.get());
    ASSERT_NE(set_knob, nullptr);
    EXPECT_EQ(set_knob->tag, T_SetKnob);
    EXPECT_EQ(set_knob->knob_, "enable_sortmerge");
    EXPECT_TRUE(set_knob->value_);
}

TEST_F(QueryPlanningTest, RejectsUnknownKnobOrValue) {
    for (const char* sql : {"set enable_hashjoin = true;", "set enable_sortmerge = yes;"}) {
        auto parsed = rucbase::parser::Parse(sql);
        ASSERT_TRUE(parsed.ok()) << sql;
        EXPECT_THROW(analyzer_.analyze(parsed.statement), InvalidKnobError) << sql;
    }
}

TEST_F(QueryPlanningTest, UsesNestedLoopJoinByDefault) {
    auto parsed = rucbase::parser::Parse("select * from a, b where a.id = b.id;");
    ASSERT_TRUE(parsed.ok());

    auto plan = planner_.do_planner(analyzer_.analyze(parsed.statement), nullptr);
    auto* join = dynamic_cast<JoinPlan*>(dynamic_cast<SelectPlan*>(plan.get())->projection_->subplan_.get());
    ASSERT_NE(join, nullptr);
    EXPECT_EQ(join->tag, T_NestLoop);
    EXPECT_NE(dynamic_cast<ScanPlan*>(join->left_.get()), nullptr);
    EXPECT_NE(dynamic_cast<ScanPlan*>(join->right_.get()), nullptr);
}

TEST_F(QueryPlanningTest, UsesSortMergeJoinForEquiJoinWhenEnabled) {
    planner_.set_enable_sortmerge(true);
    // 等值条件写在后面、列顺序与 FROM 相反，检查归并键被放到首位且 lhs 属于左孩子。
    auto parsed = rucbase::parser::Parse("select * from a, b where a.id < b.x and b.y = a.id;");
    ASSERT_TRUE(parsed.ok());

    auto plan = planner_.do_planner(analyzer_.analyze(parsed.statement), nullptr);
    auto* join = dynamic_cast<JoinPlan*>(dynamic_cast<SelectPlan*>(plan.get())->projection_->subplan_.get());
    ASSERT_NE(join, nullptr);
    EXPECT_EQ(join->tag, T_SortMerge);
    ASSERT_EQ(join->conds_.size(), 2U);
    const Condition& key = join->conds_[0];
    EXPECT_EQ(key.op, OP_EQ);
    EXPECT_FALSE(key.is_rhs_val);
    EXPECT_EQ(join->conds_[1].op, OP_LT);

    auto* left_sort = dynamic_cast<SortPlan*>(join->left_.get());
    auto* right_sort = dynamic_cast<SortPlan*>(join->right_.get());
    ASSERT_NE(left_sort, nullptr);
    ASSERT_NE(right_sort, nullptr);
    EXPECT_FALSE(left_sort->is_desc_);
    EXPECT_FALSE(right_sort->is_desc_);
    auto* left_scan = dynamic_cast<ScanPlan*>(left_sort->subplan_.get());
    auto* right_scan = dynamic_cast<ScanPlan*>(right_sort->subplan_.get());
    ASSERT_NE(left_scan, nullptr);
    ASSERT_NE(right_scan, nullptr);
    EXPECT_EQ(key.lhs_col.tab_name, left_scan->tab_name_);
    EXPECT_EQ(key.rhs_col.tab_name, right_scan->tab_name_);
    EXPECT_EQ(left_sort->sel_col_.tab_name, key.lhs_col.tab_name);
    EXPECT_EQ(left_sort->sel_col_.col_name, key.lhs_col.col_name);
    EXPECT_EQ(right_sort->sel_col_.tab_name, key.rhs_col.tab_name);
    EXPECT_EQ(right_sort->sel_col_.col_name, key.rhs_col.col_name);
}

TEST_F(QueryPlanningTest, KeepsNestedLoopJoinWithoutEquality) {
    planner_.set_enable_sortmerge(true);
    for (const char* sql : {"select * from a, b where a.id < b.id;", "select * from a, b;"}) {
        auto parsed = rucbase::parser::Parse(sql);
        ASSERT_TRUE(parsed.ok()) << sql;
        auto plan = planner_.do_planner(analyzer_.analyze(parsed.statement), nullptr);
        auto* join = dynamic_cast<JoinPlan*>(dynamic_cast<SelectPlan*>(plan.get())->projection_->subplan_.get());
        ASSERT_NE(join, nullptr) << sql;
        EXPECT_EQ(join->tag, T_NestLoop) << sql;
    }
}

TEST_F(QueryPlanningTest, UsesSortMergeJoinAtEveryLevelOfMultiTableJoin) {
    planner_.set_enable_sortmerge(true);
    auto parsed = rucbase::parser::Parse("select * from a, b, c where a.id = b.id and b.x = c.value;");
    ASSERT_TRUE(parsed.ok());

    auto plan = planner_.do_planner(analyzer_.analyze(parsed.statement), nullptr);
    auto* top = dynamic_cast<JoinPlan*>(dynamic_cast<SelectPlan*>(plan.get())->projection_->subplan_.get());
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->tag, T_SortMerge);
    int merge_joins = 0;
    for (Plan* child : {top->left_.get(), top->right_.get()}) {
        auto* sort = dynamic_cast<SortPlan*>(child);
        ASSERT_NE(sort, nullptr);
        if (auto* inner = dynamic_cast<JoinPlan*>(sort->subplan_.get())) {
            EXPECT_EQ(inner->tag, T_SortMerge);
            ++merge_joins;
        }
    }
    EXPECT_EQ(merge_joins, 1);
    EXPECT_NE(FindScan(plan.get(), "a"), nullptr);
    EXPECT_NE(FindScan(plan.get(), "b"), nullptr);
    EXPECT_NE(FindScan(plan.get(), "c"), nullptr);
}

TEST_F(QueryPlanningTest, PreservesFromTableOrderDuringAnalysis) {
    auto parsed = rucbase::parser::Parse("select a.id from a join b, c;");
    ASSERT_TRUE(parsed.ok());
    auto query = analyzer_.analyze(parsed.statement);

    EXPECT_EQ(query->bound_tables, (std::vector<std::string>{"a", "b", "c"}));
}

TEST_F(QueryPlanningTest, RejectsNonPositiveCharLengthsBeforeFileCreation) {
    auto negative_plan = PlanCreateTable("create table negative_len(c char(-1));");
    auto* negative = dynamic_cast<DDLPlan*>(negative_plan.get());
    ASSERT_NE(negative, nullptr);
    EXPECT_THROW(sm_manager_.create_table(negative->tab_name_, negative->cols_, nullptr), InvalidColLengthError);

    auto zero_plan = PlanCreateTable("create table zero_len(c char(0));");
    auto* zero = dynamic_cast<DDLPlan*>(zero_plan.get());
    ASSERT_NE(zero, nullptr);
    EXPECT_THROW(sm_manager_.create_table(zero->tab_name_, zero->cols_, nullptr), InvalidColLengthError);
}

TEST_F(QueryPlanningTest, RejectsDuplicateColumnsBeforeFileCreation) {
    auto root = PlanCreateTable("create table duplicate_cols(id int, id float);");
    auto* plan = dynamic_cast<DDLPlan*>(root.get());
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

    auto plan = planner_.do_planner(std::move(query), nullptr);
    auto* select = dynamic_cast<SelectPlan*>(plan.get());
    ASSERT_NE(select, nullptr);
    auto* sort = dynamic_cast<SortPlan*>(select->projection_->subplan_.get());
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
