// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "common/errors.h"
#include "gtest/gtest.h"
#include "index/b_plus_tree.h"
#include "storage/page.h"

namespace {

IndexFileHeader make_header(int key_len) {
    int order = static_cast<int>((PAGE_SIZE - sizeof(IndexPageHeader)) / (key_len + sizeof(Rid)) - 1);
    while (order > 2 && !IndexFileHeader::page_layout_fits(order, key_len)) {
        --order;
    }
    IndexFileHeader header(INDEX_NO_PAGE, INDEX_INITIAL_PAGE_COUNT, INDEX_INITIAL_ROOT_PAGE, 1, key_len, order,
                           IndexFileHeader::calculate_keys_region_size(order, key_len), INDEX_INITIAL_ROOT_PAGE,
                           INDEX_INITIAL_ROOT_PAGE);
    header.column_types_.push_back(TYPE_STRING);
    header.column_lengths_.push_back(key_len);
    header.update_serialized_size();
    return header;
}

TEST(IndexLayoutTest, NumericComparisonAcceptsPackedKeys) {
    alignas(int) std::array<char, 1 + sizeof(int)> int_left{};
    alignas(int) std::array<char, 1 + sizeof(int)> int_right{};
    int lhs = 7;
    int rhs = 11;
    memcpy(int_left.data() + 1, &lhs, sizeof(lhs));
    memcpy(int_right.data() + 1, &rhs, sizeof(rhs));
    EXPECT_LT(compare_index_key(int_left.data() + 1, int_right.data() + 1, TYPE_INT, sizeof(int)), 0);

    alignas(float) std::array<char, 1 + sizeof(float)> float_left{};
    alignas(float) std::array<char, 1 + sizeof(float)> float_right{};
    float flhs = 2.5F;
    float frhs = 1.5F;
    memcpy(float_left.data() + 1, &flhs, sizeof(flhs));
    memcpy(float_right.data() + 1, &frhs, sizeof(frhs));
    EXPECT_GT(compare_index_key(float_left.data() + 1, float_right.data() + 1, TYPE_FLOAT, sizeof(float)), 0);
}

TEST(IndexLayoutTest, RidAreaIsAlignedForOddLengthKeys) {
    IndexFileHeader header = make_header(3);
    Page page;
    BPlusTreeNode node(&header, &page);

    EXPECT_EQ(reinterpret_cast<uintptr_t>(node.get_rid(0)) % alignof(Rid), 0U);
    const Rid expected{.page_no = 17, .slot_no = 23};
    node.set_rid(0, expected);
    EXPECT_EQ(*node.get_rid(0), expected);
}

TEST(IndexLayoutTest, FileHeaderRoundTripsAndRejectsInvalidBounds) {
    IndexFileHeader header = make_header(3);
    std::array<char, PAGE_SIZE> page{};
    header.serialize(page.data());

    IndexFileHeader restored;
    restored.deserialize(page.data(), page.size());
    EXPECT_EQ(restored.serialized_size_, header.serialized_size_);
    EXPECT_EQ(restored.column_count_, 1);
    EXPECT_EQ(restored.key_length_, 3);
    EXPECT_EQ(restored.keys_region_size_, header.keys_region_size_);
    EXPECT_FALSE(restored.unique_);

    header.unique_ = true;
    header.serialize(page.data());
    restored = IndexFileHeader{};
    restored.deserialize(page.data(), page.size());
    EXPECT_TRUE(restored.unique_);

    IndexFileHeader truncated;
    EXPECT_THROW(truncated.deserialize(page.data(), header.serialized_size_ - 1), InternalError);

    int oversized = PAGE_SIZE + 1;
    memcpy(page.data(), &oversized, sizeof(oversized));
    IndexFileHeader invalid;
    EXPECT_THROW(invalid.deserialize(page.data(), page.size()), InternalError);

    IndexFileHeader too_many;
    too_many.column_count_ = static_cast<int>(IndexFileHeader::max_serialized_columns() + 1);
    EXPECT_THROW(too_many.update_serialized_size(), InternalError);
}

}  // namespace
