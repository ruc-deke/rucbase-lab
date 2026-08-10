// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "gtest/gtest.h"
#include "index/ix_index_handle.h"

namespace {

IxFileHdr make_header(int key_len) {
    int order = static_cast<int>((PAGE_SIZE - sizeof(IxPageHdr)) / (key_len + sizeof(Rid)) - 1);
    while (order > 2 && !IxFileHdr::page_layout_fits(order, key_len)) {
        --order;
    }
    IxFileHdr header(IX_NO_PAGE, IX_INIT_NUM_PAGES, IX_INIT_ROOT_PAGE, 1, key_len, order,
                     IxFileHdr::calculate_keys_size(order, key_len), IX_INIT_ROOT_PAGE, IX_INIT_ROOT_PAGE);
    header.col_types_.push_back(TYPE_STRING);
    header.col_lens_.push_back(key_len);
    header.update_tot_len();
    return header;
}

TEST(IxLayoutTest, NumericComparisonAcceptsPackedKeys) {
    alignas(int) std::array<char, 1 + sizeof(int)> int_left{};
    alignas(int) std::array<char, 1 + sizeof(int)> int_right{};
    int lhs = 7;
    int rhs = 11;
    memcpy(int_left.data() + 1, &lhs, sizeof(lhs));
    memcpy(int_right.data() + 1, &rhs, sizeof(rhs));
    EXPECT_LT(ix_compare(int_left.data() + 1, int_right.data() + 1, TYPE_INT, sizeof(int)), 0);

    alignas(float) std::array<char, 1 + sizeof(float)> float_left{};
    alignas(float) std::array<char, 1 + sizeof(float)> float_right{};
    float flhs = 2.5F;
    float frhs = 1.5F;
    memcpy(float_left.data() + 1, &flhs, sizeof(flhs));
    memcpy(float_right.data() + 1, &frhs, sizeof(frhs));
    EXPECT_GT(ix_compare(float_left.data() + 1, float_right.data() + 1, TYPE_FLOAT, sizeof(float)), 0);
}

TEST(IxLayoutTest, RidAreaIsAlignedForOddLengthKeys) {
    IxFileHdr header = make_header(3);
    Page page;
    IxNodeHandle node(&header, &page);

    EXPECT_EQ(reinterpret_cast<uintptr_t>(node.get_rid(0)) % alignof(Rid), 0U);
    const Rid expected{.page_no = 17, .slot_no = 23};
    node.set_rid(0, expected);
    EXPECT_EQ(*node.get_rid(0), expected);
}

TEST(IxLayoutTest, FileHeaderRoundTripsAndRejectsInvalidBounds) {
    IxFileHdr header = make_header(3);
    std::array<char, PAGE_SIZE> page{};
    header.serialize(page.data());

    IxFileHdr restored;
    restored.deserialize(page.data(), page.size());
    EXPECT_EQ(restored.tot_len_, header.tot_len_);
    EXPECT_EQ(restored.col_num_, 1);
    EXPECT_EQ(restored.col_tot_len_, 3);
    EXPECT_EQ(restored.keys_size_, header.keys_size_);

    IxFileHdr truncated;
    EXPECT_THROW(truncated.deserialize(page.data(), header.tot_len_ - 1), InternalError);

    int oversized = PAGE_SIZE + 1;
    memcpy(page.data(), &oversized, sizeof(oversized));
    IxFileHdr invalid;
    EXPECT_THROW(invalid.deserialize(page.data(), page.size()), InternalError);

    IxFileHdr too_many;
    too_many.col_num_ = static_cast<int>(IxFileHdr::max_serialized_col_num() + 1);
    EXPECT_THROW(too_many.update_tot_len(), InternalError);
}

}  // namespace
