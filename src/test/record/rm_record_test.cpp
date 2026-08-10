// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <utility>

#include "record/rm_defs.h"

TEST(RmRecordTest, SupportsCopyMoveAndSelfAssignment) {
    const std::array<char, 3> bytes{{'a', 'b', 'c'}};
    RmRecord source(static_cast<int>(bytes.size()), bytes.data());

    RmRecord copy(source);
    ASSERT_EQ(copy.size, 3);
    EXPECT_EQ(std::memcmp(copy.data, bytes.data(), bytes.size()), 0);
    EXPECT_NE(copy.data, source.data);

    RmRecord* same_record = &copy;
    copy = *same_record;
    EXPECT_EQ(std::memcmp(copy.data, bytes.data(), bytes.size()), 0);

    RmRecord assigned(1);
    assigned = source;
    ASSERT_EQ(assigned.size, 3);
    EXPECT_EQ(std::memcmp(assigned.data, bytes.data(), bytes.size()), 0);

    RmRecord moved(std::move(assigned));
    // RmRecord 明确保证移动后为空，这里有意验证该类型自己的契约。
    EXPECT_EQ(assigned.data, nullptr);  // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(assigned.size, 0);        // NOLINT(bugprone-use-after-move)
    EXPECT_EQ(std::memcmp(moved.data, bytes.data(), bytes.size()), 0);
}

TEST(RmRecordTest, DeserializesUnalignedInputAndValidatesLength) {
    std::array<char, sizeof(int) + 4> serialized{};
    const int size = 3;
    std::memcpy(serialized.data() + 1, &size, sizeof(size));
    std::memcpy(serialized.data() + 1 + sizeof(size), "xyz", size);

    RmRecord record;
    record.Deserialize(serialized.data() + 1);
    ASSERT_EQ(record.size, size);
    EXPECT_EQ(std::memcmp(record.data, "xyz", size), 0);

    std::array<char, sizeof(int)> invalid{};
    const int oversized = RM_MAX_RECORD_SIZE + 1;
    std::memcpy(invalid.data(), &oversized, sizeof(oversized));
    EXPECT_THROW(record.Deserialize(invalid.data()), InvalidRecordSizeError);
    EXPECT_THROW(RmRecord(-1), InvalidRecordSizeError);
}

TEST(PageIdTest, OrdersByFileThenPage) {
    const PageId first{.fd = 1, .page_no = 100};
    const PageId second{.fd = 2, .page_no = 0};
    EXPECT_TRUE(first < second);
    EXPECT_FALSE(second < first);
}
