// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "record/bitmap.h"
#include "record/rm_file_handle.h"
#include "storage/buffer_pool_manager.h"
#include "storage/disk_manager.h"

class RmPageGuardTest : public ::testing::Test {
protected:
    void SetUp() override {
        disk_manager_ = std::make_unique<DiskManager>();
        if (disk_manager_->is_file(file_name_)) {
            disk_manager_->destroy_file(file_name_);
        }
        disk_manager_->create_file(file_name_);
        file_descriptor_ = disk_manager_->open_file(file_name_);
        buffer_pool_manager_ = std::make_unique<BufferPoolManager>(2, disk_manager_.get());
        file_hdr_.record_size = 8;
        file_hdr_.num_records_per_page = 8;
        file_hdr_.bitmap_size = 1;
        file_hdr_.num_pages = 1;
        file_hdr_.first_free_page_no = RM_NO_PAGE;
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

    const std::string file_name_ = "rm_page_guard_test.db";
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> buffer_pool_manager_;
    int file_descriptor_ = -1;
    RmFileHdr file_hdr_{};
};

TEST_F(RmPageGuardTest, ReleasesPinAndPropagatesDirty) {
    const PinSnapshot before_pin = buffer_pool_manager_->get_pin_snapshot();
    PageId page_id{.fd = file_descriptor_, .page_no = INVALID_PAGE_ID};
    PageGuard guard = buffer_pool_manager_->new_page_guard(&page_id);
    if (!guard) {
        GTEST_SKIP() << "Lab 1 buffer pool is not implemented";
    }
    const PinSnapshot held = buffer_pool_manager_->get_pin_snapshot();
    EXPECT_NE(held, before_pin);

    {
        RmPageGuard page(&file_hdr_, std::move(guard));
        ASSERT_TRUE(page.valid());
        Bitmap::set(page->bitmap, 0);
        page.mark_dirty();
        EXPECT_TRUE(Bitmap::is_set(page->bitmap, 0));
        EXPECT_EQ(buffer_pool_manager_->get_pin_snapshot(), held);
    }
    EXPECT_EQ(buffer_pool_manager_->get_pin_snapshot(), before_pin);

    PageGuard fetched = buffer_pool_manager_->fetch_page_guard(page_id);
    if (!fetched) {
        GTEST_SKIP() << "Lab 1 fetch_page is not implemented";
    }
    EXPECT_TRUE(fetched->is_dirty());
}

TEST_F(RmPageGuardTest, ExceptionUnpins) {
    const PinSnapshot before_pin = buffer_pool_manager_->get_pin_snapshot();
    PageId page_id{.fd = file_descriptor_, .page_no = INVALID_PAGE_ID};
    PageGuard guard = buffer_pool_manager_->new_page_guard(&page_id);
    if (!guard) {
        GTEST_SKIP() << "Lab 1 buffer pool is not implemented";
    }
    const PinSnapshot held = buffer_pool_manager_->get_pin_snapshot();
    EXPECT_NE(held, before_pin);
    try {
        RmPageGuard page(&file_hdr_, std::move(guard));
        throw std::runtime_error("record page guard");
    } catch (const std::runtime_error&) {  // NOLINT(bugprone-empty-catch)
    }
    EXPECT_EQ(buffer_pool_manager_->get_pin_snapshot(), before_pin);
}
