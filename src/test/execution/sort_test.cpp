// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

/**
 * @file sort_test.cpp
 * @brief Lab 3 排序算子测试。
 */

#include <algorithm>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "execution/execution_sort.h"
#include "gtest/gtest.h"
#include "mock_executor.h"

namespace {

using rucbase::test::MockExecutor;
using rucbase::test::Row;

std::vector<ColMeta> table_schema() {
    return rucbase::test::make_schema(
        "t", {{"id", {TYPE_INT, 4}}, {"score", {TYPE_FLOAT, 4}}, {"name", {TYPE_STRING, 6}}});
}

/** @brief 对 t 的 column 列排序，检查输出是输入的重排且按该列有序，并且孩子只扫描一次。 */
void check_sort(const std::vector<Row>& input, const std::string& column, size_t column_index, bool is_desc) {
    auto child = std::make_unique<MockExecutor>(table_schema(), input);
    MockExecutor* child_raw = child.get();
    SortExecutor sort(std::move(child), {.tab_name = "t", .col_name = column}, is_desc);

    ASSERT_EQ(sort.tuple_len(), rucbase::test::schema_len(table_schema()));
    ASSERT_EQ(sort.cols().size(), table_schema().size());
    const std::vector<Row> output = rucbase::test::drain(sort);

    std::vector<Row> actual_sorted = output;
    std::vector<Row> expected_sorted = input;
    std::ranges::sort(actual_sorted);
    std::ranges::sort(expected_sorted);
    EXPECT_EQ(actual_sorted, expected_sorted) << "sort output must be a permutation of its input";

    const auto key = [column_index](const Row& row) { return row[column_index]; };
    if (is_desc) {
        EXPECT_TRUE(std::ranges::is_sorted(output, std::ranges::greater{}, key));
    } else {
        EXPECT_TRUE(std::ranges::is_sorted(output, std::ranges::less{}, key));
    }
    EXPECT_EQ(child_raw->begin_calls(), 1) << "sort should read its child exactly once";
}

std::vector<Row> sample_rows() {
    return {{3, 1.5F, std::string("carol")}, {1, 9.0F, std::string("alice")}, {2, -2.0F, std::string("bob")},
            {3, 0.5F, std::string("dave")},  {1, 9.0F, std::string("erin")},  {0, 4.25F, std::string("frank")}};
}

/** @note lab3 计分：1 point */
TEST(SortExecutorTest, SortsAscendingWithDuplicates) { check_sort(sample_rows(), "id", 0, false); }

/** @note lab3 计分：1 point */
TEST(SortExecutorTest, SortsDescending) { check_sort(sample_rows(), "id", 0, true); }

/** @note lab3 计分：1 point */
TEST(SortExecutorTest, SortsByNonFirstFloatAndStringColumns) {
    check_sort(sample_rows(), "score", 1, false);
    check_sort(sample_rows(), "name", 2, false);
    check_sort(sample_rows(), "name", 2, true);
}

/** @note lab3 计分：1 point */
TEST(SortExecutorTest, HandlesEmptyAndSingleRowInputs) {
    check_sort({}, "id", 0, false);
    check_sort({{7, 1.0F, std::string("solo")}}, "id", 0, true);
}

/** @note lab3 计分：1 point */
TEST(SortExecutorTest, SortsLargerRandomInput) {
    std::mt19937 rng(7);
    std::vector<Row> rows;
    rows.reserve(500);
    for (int i = 0; i < 500; ++i) {
        rows.push_back({static_cast<int>(rng() % 50), static_cast<float>(rng() % 1000) / 8.0F,
                        std::string(1, static_cast<char>('a' + rng() % 26))});
    }
    check_sort(rows, "id", 0, false);
    check_sort(rows, "score", 1, true);
}

}  // namespace
