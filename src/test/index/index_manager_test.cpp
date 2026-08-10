// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "index/index_manager.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "index/b_plus_tree.h"
#include "storage/buffer_pool_manager.h"
#include "storage/disk_manager.h"
#include "system/sm_meta.h"

namespace {

const std::string kTableName = "index_reopen_table";
const std::vector<ColMeta> kIndexCols = {
    {.tab_name = kTableName, .name = "id", .type = TYPE_INT, .len = sizeof(int), .offset = 0, .index = true},
};

TEST(IndexManagerTest, ReopenRestoresPageAllocatorHighWaterMark) {
    std::string index_name;
    {
        DiskManager disk_manager;
        BufferPoolManager buffer_pool_manager(8, &disk_manager);
        IndexManager index_manager(&disk_manager, &buffer_pool_manager);
        index_name = index_manager.make_index_name(kTableName, kIndexCols);
        if (disk_manager.is_file(index_name)) {
            disk_manager.destroy_file(index_name);
        }
        index_manager.create_index(kTableName, kIndexCols);
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
        IndexManager index_manager(&disk_manager, &buffer_pool_manager);
        auto index = index_manager.open_index(kTableName, kIndexCols);
        int file_descriptor = disk_manager.get_file_fd(index_name);

        EXPECT_EQ(disk_manager.allocate_page(file_descriptor), INDEX_INITIAL_PAGE_COUNT);

        // Extend the file without updating page_count_ to model a stale header.
        std::array<char, PAGE_SIZE> page{};
        disk_manager.write_page(file_descriptor, INDEX_INITIAL_PAGE_COUNT, page.data(), page.size());
        index_manager.close_index(index.get());
    }

    // Physical size is authoritative when it is ahead of the persisted header.
    {
        DiskManager disk_manager;
        BufferPoolManager buffer_pool_manager(8, &disk_manager);
        IndexManager index_manager(&disk_manager, &buffer_pool_manager);
        auto index = index_manager.open_index(kTableName, kIndexCols);
        int file_descriptor = disk_manager.get_file_fd(index_name);

        EXPECT_EQ(disk_manager.allocate_page(file_descriptor), INDEX_INITIAL_PAGE_COUNT + 1);

        index_manager.close_index(index.get());
        index_manager.destroy_index(kTableName, kIndexCols);
    }
}

}  // namespace
