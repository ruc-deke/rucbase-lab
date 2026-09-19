// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

/**
 * @file merge_join_test.cpp
 * @brief Lab 3 排序归并连接算子测试。
 *
 * 左右孩子是按连接键升序输出的内存数据源。测试检查：
 *   1. 输出与嵌套循环语义下的结果（多重集）完全一致；
 *   2. 输出按连接键非递减，这是归并连接天然具有的性质；
 *   3. 每个孩子只被扫描一遍（begin_tuple() 恰好调用一次）。
 */

#include <algorithm>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "execution/executor_merge_join.h"
#include "gtest/gtest.h"
#include "mock_executor.h"

namespace {

using rucbase::test::Cell;
using rucbase::test::MockExecutor;
using rucbase::test::Row;

Condition column_condition(const std::string& lhs_tab,
                           const std::string& lhs_col,
                           CompOp op,
                           const std::string& rhs_tab,
                           const std::string& rhs_col) {
    Condition condition;
    condition.lhs_col = {.tab_name = lhs_tab, .col_name = lhs_col};
    condition.op = op;
    condition.is_rhs_val = false;
    condition.rhs_col = {.tab_name = rhs_tab, .col_name = rhs_col};
    return condition;
}

bool holds(const Cell& lhs, CompOp op, const Cell& rhs) {
    switch (op) {
        case OP_EQ:
            return lhs == rhs;
        case OP_NE:
            return lhs != rhs;
        case OP_LT:
            return lhs < rhs;
        case OP_GT:
            return lhs > rhs;
        case OP_LE:
            return lhs <= rhs;
        case OP_GE:
            return lhs >= rhs;
    }
    return false;
}

/** @brief 左表 l(k, v)、右表 r(k, w) 的一次连接测试。k 的类型由参数决定，v/w 为 int。 */
class MergeJoinCase {
public:
    MergeJoinCase(ColType key_type, int key_len) {
        left_cols_ = rucbase::test::make_schema("l", {{"k", {key_type, key_len}}, {"v", {TYPE_INT, 4}}});
        right_cols_ = rucbase::test::make_schema("r", {{"k", {key_type, key_len}}, {"w", {TYPE_INT, 4}}});
        conds_.push_back(column_condition("l", "k", OP_EQ, "r", "k"));
    }

    /** @brief 追加一个 l.v op r.w 的附加连接条件。 */
    void add_residual(CompOp op) { conds_.push_back(column_condition("l", "v", op, "r", "w")); }

    /** @brief 运行归并连接并检查结果；left/right 必须已按 k 升序排列。 */
    void run(const std::vector<Row>& left, const std::vector<Row>& right) {
        auto left_child = std::make_unique<MockExecutor>(left_cols_, left);
        auto right_child = std::make_unique<MockExecutor>(right_cols_, right);
        MockExecutor* left_raw = left_child.get();
        MockExecutor* right_raw = right_child.get();
        MergeJoinExecutor join(std::move(left_child), std::move(right_child), conds_);

        ASSERT_EQ(join.tuple_len(), rucbase::test::schema_len(left_cols_) + rucbase::test::schema_len(right_cols_));
        ASSERT_EQ(join.cols().size(), 4U);
        const std::vector<Row> output = rucbase::test::drain(join);

        std::vector<Row> expected;
        for (const Row& l : left) {
            for (const Row& r : right) {
                bool match = l[0] == r[0];
                for (size_t i = 1; i < conds_.size() && match; ++i) {
                    match = holds(l[1], conds_[i].op, r[1]);
                }
                if (match) {
                    expected.push_back({l[0], l[1], r[0], r[1]});
                }
            }
        }

        std::vector<Row> actual_sorted = output;
        std::ranges::sort(actual_sorted);
        std::ranges::sort(expected);
        EXPECT_EQ(actual_sorted, expected) << "output differs from the nested-loop result";
        EXPECT_TRUE(std::ranges::is_sorted(output, {}, [](const Row& row) { return row[0]; }))
            << "merge join output should be ordered by the join key";
        EXPECT_EQ(left_raw->begin_calls(), 1) << "left child should be scanned exactly once";
        EXPECT_EQ(right_raw->begin_calls(), 1) << "right child should be scanned exactly once";
    }

private:
    std::vector<ColMeta> left_cols_;
    std::vector<ColMeta> right_cols_;
    std::vector<Condition> conds_;
};

std::vector<Row> int_rows(const std::vector<int>& keys, int first_payload) {
    std::vector<Row> rows;
    rows.reserve(keys.size());
    for (int key : keys) {
        rows.push_back({key, first_payload++});
    }
    return rows;
}

/** @note lab3 计分：1 point */
TEST(MergeJoinTest, JoinsUniqueKeys) {
    MergeJoinCase test(TYPE_INT, 4);
    test.run(int_rows({1, 2, 3, 5}, 0), int_rows({2, 3, 4, 5}, 100));
}

/** @note lab3 计分：1 point */
TEST(MergeJoinTest, JoinsDuplicateKeysOnOneSide) {
    MergeJoinCase left_duplicates(TYPE_INT, 4);
    left_duplicates.run(int_rows({1, 2, 2, 2, 3}, 0), int_rows({2, 3}, 100));

    MergeJoinCase right_duplicates(TYPE_INT, 4);
    right_duplicates.run(int_rows({2, 3}, 0), int_rows({1, 2, 2, 2, 3, 3}, 100));
}

/** @note lab3 计分：2 points */
TEST(MergeJoinTest, JoinsDuplicateKeysOnBothSides) {
    MergeJoinCase test(TYPE_INT, 4);
    test.run(int_rows({1, 2, 2, 3, 3, 3, 7}, 0), int_rows({2, 2, 2, 3, 3, 6, 7, 7}, 100));
}

/** @note lab3 计分：1 point */
TEST(MergeJoinTest, HandlesNoMatchesAndEmptyInputs) {
    MergeJoinCase disjoint(TYPE_INT, 4);
    disjoint.run(int_rows({1, 3, 5}, 0), int_rows({2, 4, 6}, 100));

    MergeJoinCase left_empty(TYPE_INT, 4);
    left_empty.run({}, int_rows({1, 2}, 100));

    MergeJoinCase right_empty(TYPE_INT, 4);
    right_empty.run(int_rows({1, 2}, 0), {});

    MergeJoinCase both_empty(TYPE_INT, 4);
    both_empty.run({}, {});
}

/** @note lab3 计分：1 point */
TEST(MergeJoinTest, HandlesGroupsAtBothEnds) {
    MergeJoinCase first_and_last(TYPE_INT, 4);
    first_and_last.run(int_rows({1, 1, 1, 5, 9}, 0), int_rows({0, 1, 1, 9, 9, 9, 9}, 100));

    MergeJoinCase one_side_exhausted_first(TYPE_INT, 4);
    one_side_exhausted_first.run(int_rows({4, 4}, 0), int_rows({1, 2, 3, 4, 4, 8, 9}, 100));
}

/** @note lab3 计分：1 point */
TEST(MergeJoinTest, JoinsFloatAndStringKeys) {
    MergeJoinCase float_keys(TYPE_FLOAT, 4);
    float_keys.run({{-1.5F, 0}, {0.0F, 1}, {2.25F, 2}, {2.25F, 3}}, {{0.0F, 100}, {2.25F, 101}, {3.0F, 102}});

    MergeJoinCase string_keys(TYPE_STRING, 8);
    string_keys.run({{std::string("apple"), 0}, {std::string("kiwi"), 1}, {std::string("kiwi"), 2},
                     {std::string("pear"), 3}},
                    {{std::string("apple"), 100}, {std::string("fig"), 101}, {std::string("kiwi"), 102}});
}

/** @note lab3 计分：1 point */
TEST(MergeJoinTest, AppliesResidualConditions) {
    MergeJoinCase test(TYPE_INT, 4);
    test.add_residual(OP_LT);
    test.run({{1, 5}, {2, 1}, {2, 7}, {2, 9}, {3, 4}}, {{1, 3}, {2, 2}, {2, 8}, {3, 4}, {3, 5}});
}

/** @note lab3 计分：2 points */
TEST(MergeJoinTest, MatchesNestedLoopOnRandomInputs) {
    std::mt19937 rng(20260918);
    for (int round = 0; round < 30; ++round) {
        const int key_range = 1 + static_cast<int>(rng() % 12);
        std::vector<int> left_keys(rng() % 60);
        std::vector<int> right_keys(rng() % 60);
        for (int& key : left_keys) key = static_cast<int>(rng() % key_range);
        for (int& key : right_keys) key = static_cast<int>(rng() % key_range);
        std::ranges::sort(left_keys);
        std::ranges::sort(right_keys);

        SCOPED_TRACE("round " + std::to_string(round));
        MergeJoinCase test(TYPE_INT, 4);
        if (round % 3 == 0) test.add_residual(OP_NE);
        test.run(int_rows(left_keys, 0), int_rows(right_keys, 1000));
        if (::testing::Test::HasFatalFailure()) return;
    }
}

}  // namespace
