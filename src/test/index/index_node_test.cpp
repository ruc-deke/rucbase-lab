// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "index/b_plus_tree.h"
#include "storage/buffer_pool_manager.h"
#include "storage/disk_manager.h"
#include "storage/page_guard.h"

TEST(IndexNodeTest, EmptyNodeIsInvalid) {
    IndexNode node;
    EXPECT_FALSE(node.valid());
}

TEST(IndexNodeTest, MoveLeavesSourceInvalid) {
    IndexFileHeader header{};
    IndexNode source(&header, PageGuard{});
    EXPECT_FALSE(source.valid());

    IndexNode destination = std::move(source);
    EXPECT_FALSE(source.valid());  // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    EXPECT_FALSE(destination.valid());
}

TEST(IndexNodeTest, DestructorRunsAfterException) {
    IndexFileHeader header{};
    try {
        IndexNode node(&header, PageGuard{});
        throw std::runtime_error("index node scope");
    } catch (const std::runtime_error&) {  // NOLINT(bugprone-empty-catch)
    }
}

class IndexNodePinTest : public ::testing::Test {
protected:
    void SetUp() override {
        disk_manager_ = std::make_unique<DiskManager>();
        if (disk_manager_->is_file(file_name_)) {
            disk_manager_->destroy_file(file_name_);
        }
        disk_manager_->create_file(file_name_);
        file_descriptor_ = disk_manager_->open_file(file_name_);
        buffer_pool_manager_ = std::make_unique<BufferPoolManager>(2, disk_manager_.get());
        header_.key_length_ = static_cast<int>(sizeof(int));
        header_.tree_order_ = 4;
        header_.keys_region_size_ =
            IndexFileHeader::calculate_keys_region_size(header_.tree_order_, header_.key_length_);
    }

    void TearDown() override {
        if (file_descriptor_ >= 0) {
            buffer_pool_manager_->flush_all_pages(file_descriptor_);
            disk_manager_->close_file(file_descriptor_);
        }
        buffer_pool_manager_.reset();
        if (disk_manager_ && disk_manager_->is_file(file_name_)) {
            disk_manager_->destroy_file(file_name_);
        }
    }

    PageGuard new_page() {
        PageId page_id{.fd = file_descriptor_, .page_no = INVALID_PAGE_ID};
        return buffer_pool_manager_->new_page_guard(&page_id);
    }

    const std::string file_name_ = "index_node_pin_test.db";
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> buffer_pool_manager_;
    int file_descriptor_ = -1;
    IndexFileHeader header_{};
};

TEST_F(IndexNodePinTest, ReleasesPinOnMoveAndDestroy) {
    const PinSnapshot before_pin = buffer_pool_manager_->get_pin_snapshot();
    PageGuard guard = new_page();
    if (!guard) {
        GTEST_SKIP() << "Lab 1 buffer pool is not implemented";
    }
    const PinSnapshot held = buffer_pool_manager_->get_pin_snapshot();
    EXPECT_NE(held, before_pin);

    {
        IndexNode node(&header_, std::move(guard));
        ASSERT_TRUE(node.valid());
        node.mark_dirty();
        IndexNode moved = std::move(node);
        EXPECT_FALSE(node.valid());  // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
        ASSERT_TRUE(moved.valid());
        EXPECT_EQ(buffer_pool_manager_->get_pin_snapshot(), held);
        std::memcpy(moved->get_key(0), "\x01\x00\x00\x00", sizeof(int));
    }

    EXPECT_EQ(buffer_pool_manager_->get_pin_snapshot(), before_pin);
}

TEST_F(IndexNodePinTest, ExceptionUnpinsAndDirtyPropagates) {
    const PinSnapshot before_pin = buffer_pool_manager_->get_pin_snapshot();
    PageGuard guard = new_page();
    if (!guard) {
        GTEST_SKIP() << "Lab 1 buffer pool is not implemented";
    }
    const PageId page_id = guard.page_id();
    const PinSnapshot held = buffer_pool_manager_->get_pin_snapshot();
    EXPECT_NE(held, before_pin);

    try {
        IndexNode node(&header_, std::move(guard));
        node.mark_dirty();
        throw std::runtime_error("index node exception");
    } catch (const std::runtime_error&) {  // NOLINT(bugprone-empty-catch)
    }
    EXPECT_EQ(buffer_pool_manager_->get_pin_snapshot(), before_pin);

    PageGuard fetched = buffer_pool_manager_->fetch_page_guard(page_id);
    if (!fetched) {
        GTEST_SKIP() << "Lab 1 fetch_page is not implemented";
    }
    EXPECT_TRUE(fetched->is_dirty());
}

TEST_F(IndexNodePinTest, ReleaseHandsPinToCaller) {
    const PinSnapshot before_pin = buffer_pool_manager_->get_pin_snapshot();
    PageGuard guard = new_page();
    if (!guard) {
        GTEST_SKIP() << "Lab 1 buffer pool is not implemented";
    }
    const PinSnapshot held = buffer_pool_manager_->get_pin_snapshot();
    PageGuard held_pin;
    {
        IndexNode node(&header_, std::move(guard));
        held_pin = node.release();
        EXPECT_FALSE(node.valid());  // NOLINT(bugprone-use-after-move)
        EXPECT_TRUE(static_cast<bool>(held_pin));
        EXPECT_EQ(buffer_pool_manager_->get_pin_snapshot(), held);
    }
    EXPECT_EQ(buffer_pool_manager_->get_pin_snapshot(), held);
    EXPECT_TRUE(held_pin.drop());
    EXPECT_EQ(buffer_pool_manager_->get_pin_snapshot(), before_pin);
}
