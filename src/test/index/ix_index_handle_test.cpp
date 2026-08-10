// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "index/ix.h"

namespace {

const std::string kTableName = "ix_reopen_table";
const std::vector<ColMeta> kIndexCols = {
    {.tab_name = kTableName, .name = "id", .type = TYPE_INT, .len = sizeof(int), .offset = 0, .index = true},
};

TEST(IndexHandleTest, ReopenRestoresPageAllocatorHighWaterMark) {
    std::string index_name;
    {
        DiskManager disk_manager;
        BufferPoolManager buffer_pool_manager(8, &disk_manager);
        IxManager ix_manager(&disk_manager, &buffer_pool_manager);
        index_name = ix_manager.get_index_name(kTableName, kIndexCols);
        if (disk_manager.is_file(index_name)) {
            disk_manager.destroy_file(index_name);
        }
        ix_manager.create_index(kTableName, kIndexCols);
        // This regression test becomes runnable after the Lab1 disk layer is
        // supplied; keep the scaffold suite green before that prerequisite.
        if (!disk_manager.is_file(index_name)) {
            GTEST_SKIP() << "requires the Lab1 DiskManager implementation";
        }
    }

    // A fresh DiskManager starts with an empty in-memory allocator. Opening
    // the index must recover page 3 as the first free page from disk metadata.
    {
        DiskManager disk_manager;
        BufferPoolManager buffer_pool_manager(8, &disk_manager);
        IxManager ix_manager(&disk_manager, &buffer_pool_manager);
        auto index_handle = ix_manager.open_index(kTableName, kIndexCols);
        int fd = disk_manager.get_file_fd(index_name);

        EXPECT_EQ(disk_manager.allocate_page(fd), IX_INIT_NUM_PAGES);

        // Extend the file without updating num_pages_ to model a stale header.
        std::array<char, PAGE_SIZE> page{};
        disk_manager.write_page(fd, IX_INIT_NUM_PAGES, page.data(), page.size());
        ix_manager.close_index(index_handle.get());
    }

    // Physical size is authoritative when it is ahead of the persisted header.
    {
        DiskManager disk_manager;
        BufferPoolManager buffer_pool_manager(8, &disk_manager);
        IxManager ix_manager(&disk_manager, &buffer_pool_manager);
        auto index_handle = ix_manager.open_index(kTableName, kIndexCols);
        int fd = disk_manager.get_file_fd(index_name);

        EXPECT_EQ(disk_manager.allocate_page(fd), IX_INIT_NUM_PAGES + 1);

        ix_manager.close_index(index_handle.get());
        ix_manager.destroy_index(kTableName, kIndexCols);
    }
}

}  // namespace
