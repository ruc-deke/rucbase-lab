// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <gtest/gtest.h>

#include <array>

#include "record/rm_file_handle.h"

/** @return 按照教学文档中的页面布局计算一组槽位需要的总字节数。 */
int required_page_bytes(int record_size, int slot_count) {
    int bitmap_size = (slot_count + BITMAP_WIDTH - 1) / BITMAP_WIDTH;
    const int record_bytes = record_size * slot_count;
    return static_cast<int>(Page::OFFSET_PAGE_HDR) + static_cast<int>(sizeof(RmPageHdr)) + bitmap_size + record_bytes;
}

TEST(RecordPageLayoutTest, CapacityFillsThePageWithoutOverflow) {
    for (int record_size : std::array{1, 4, 64, 256, RM_MAX_RECORD_SIZE}) {
        int slot_count = (PAGE_SIZE - Page::OFFSET_PAGE_HDR - static_cast<int>(sizeof(RmPageHdr))) / record_size;
        while (required_page_bytes(record_size, slot_count) > PAGE_SIZE) {
            slot_count--;
        }

        EXPECT_LE(required_page_bytes(record_size, slot_count), PAGE_SIZE);
        EXPECT_GT(required_page_bytes(record_size, slot_count + 1), PAGE_SIZE);
    }
}

TEST(RecordPageLayoutTest, PageHandlePointsToHeaderBitmapAndSlots) {
    RmFileHdr file_hdr{};
    file_hdr.record_size = 16;
    file_hdr.num_pages = RM_FIRST_RECORD_PAGE;
    file_hdr.num_records_per_page = 100;
    file_hdr.bitmap_size = (file_hdr.num_records_per_page + BITMAP_WIDTH - 1) / BITMAP_WIDTH;

    Page page;
    RmPageHandle page_handle(&file_hdr, &page);

    EXPECT_EQ(reinterpret_cast<char*>(page_handle.page_hdr), page.get_data() + Page::OFFSET_PAGE_HDR);
    EXPECT_EQ(page_handle.bitmap, page.get_data() + Page::OFFSET_PAGE_HDR + sizeof(RmPageHdr));
    EXPECT_EQ(page_handle.slots, page_handle.bitmap + file_hdr.bitmap_size);
    EXPECT_EQ(page_handle.get_slot(1), page_handle.slots + file_hdr.record_size);
}
