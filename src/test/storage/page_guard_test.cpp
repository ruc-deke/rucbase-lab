// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "storage/page_guard.h"

#include <cstring>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "gtest/gtest.h"
#include "storage/buffer_pool_manager.h"
#include "storage/disk_manager.h"

static_assert(!std::is_copy_constructible_v<PageGuard>);
static_assert(!std::is_copy_assignable_v<PageGuard>);
static_assert(std::is_nothrow_move_constructible_v<PageGuard>);
static_assert(std::is_nothrow_move_assignable_v<PageGuard>);

TEST(PageGuardValueTest, DefaultAndMovedFromGuardsAreHarmless) {
    PageGuard guard;
    EXPECT_FALSE(guard);
    EXPECT_EQ(guard.get(), nullptr);
    EXPECT_EQ(guard.page_id().fd, -1);
    EXPECT_EQ(guard.page_id().page_no, INVALID_PAGE_ID);
    guard.mark_dirty();
    EXPECT_TRUE(guard.drop());

    PageGuard moved = std::move(guard);
    EXPECT_FALSE(guard);  // NOLINT(bugprone-use-after-move): verifies the documented moved-from state.
    EXPECT_FALSE(moved);
    EXPECT_TRUE(moved.drop());
}

class PageGuardTest : public ::testing::Test {
protected:
    void SetUp() override {
        disk_manager_ = std::make_unique<DiskManager>();
        if (disk_manager_->is_file(file_name_)) {
            disk_manager_->destroy_file(file_name_);
        }
        disk_manager_->create_file(file_name_);
        file_descriptor_ = disk_manager_->open_file(file_name_);
        buffer_pool_manager_ = std::make_unique<BufferPoolManager>(2, disk_manager_.get());
    }

    void TearDown() override {
        if (file_descriptor_ >= 0) {
            buffer_pool_manager_->flush_all_pages(file_descriptor_);
            disk_manager_->close_file(file_descriptor_);
        }
    }

    const std::string file_name_ = "page_guard_test.db";
    std::unique_ptr<DiskManager> disk_manager_;
    std::unique_ptr<BufferPoolManager> buffer_pool_manager_;
    int file_descriptor_ = -1;
};

TEST_F(PageGuardTest, ScopeAndExplicitDropReleaseExactlyOnePin) {
    const PinSnapshot baseline = buffer_pool_manager_->get_pin_snapshot();
    PageId page_id{.fd = file_descriptor_, .page_no = INVALID_PAGE_ID};

    {
        PageGuard guard = buffer_pool_manager_->new_page_guard(&page_id);
        ASSERT_TRUE(guard);
        EXPECT_EQ(guard.page_id().fd, page_id.fd);
        EXPECT_EQ(guard.page_id().page_no, page_id.page_no);
        std::memcpy(guard->get_data(), "guard", sizeof("guard"));
        guard.mark_dirty();

        PageGuard moved = std::move(guard);
        EXPECT_FALSE(guard);  // NOLINT(bugprone-use-after-move): verifies the documented moved-from state.
        ASSERT_TRUE(moved);
        EXPECT_EQ(std::strcmp((*moved).get_data(), "guard"), 0);
    }

    EXPECT_EQ(buffer_pool_manager_->get_pin_snapshot(), baseline);

    PageGuard fetched = buffer_pool_manager_->fetch_page_guard(page_id);
    ASSERT_TRUE(fetched);
    EXPECT_EQ(std::strcmp(fetched->get_data(), "guard"), 0);
    EXPECT_TRUE(fetched->is_dirty());
    EXPECT_TRUE(fetched.drop());
    EXPECT_TRUE(fetched.drop());
    EXPECT_EQ(buffer_pool_manager_->get_pin_snapshot(), baseline);
}

TEST_F(PageGuardTest, MoveAssignmentReleasesThePreviousPin) {
    const PinSnapshot baseline = buffer_pool_manager_->get_pin_snapshot();
    PageId first_id{.fd = file_descriptor_, .page_no = INVALID_PAGE_ID};
    PageId second_id{.fd = file_descriptor_, .page_no = INVALID_PAGE_ID};
    PageGuard first = buffer_pool_manager_->new_page_guard(&first_id);
    PageGuard second = buffer_pool_manager_->new_page_guard(&second_id);
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    first = std::move(second);
    EXPECT_TRUE(first);
    EXPECT_FALSE(second);  // NOLINT(bugprone-use-after-move): verifies the documented moved-from state.
    EXPECT_EQ(first.page_id().page_no, second_id.page_no);
    PinSnapshot expected = baseline;
    ++expected[second_id];
    EXPECT_EQ(buffer_pool_manager_->get_pin_snapshot(), expected);

    EXPECT_TRUE(first.drop());
    EXPECT_EQ(buffer_pool_manager_->get_pin_snapshot(), baseline);
}
