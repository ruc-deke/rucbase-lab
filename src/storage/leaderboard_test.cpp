/**
 * Test for calculating the milliseconds time for running
 * NewPage, UnpinPage, FetchPage, DeletePage in Buffer Pool Manager.
 */

#include <cstdio>
#include <thread>  // NOLINT
#include <vector>

#include "buffer_pool_manager.h"
#include "disk_manager.h"
#include "gtest/gtest.h"

const std::string TEST_DB_NAME = "BufferPoolManagerTest_db";  // 以数据库名作为根目录
const std::string TEST_FILE_NAME = "bigdata";                 // 测试文件的名字

/** 注意：每个测试点只测试了单个文件！
 * 对于每个测试点，先创建和进入目录TEST_DB_NAME
 * 然后在此目录下创建和打开文件TEST_FILE_NAME，记录其文件描述符fd */

// Add by jiawen
class LeaderboardTest : public ::testing::Test {
   public:
    std::unique_ptr<DiskManager> disk_manager_;
    int fd_ = -1;  // 此文件描述符为disk_manager_->open_file的返回值

   public:
    // This function is called before every test.
    void SetUp() override {
        ::testing::Test::SetUp();
        // For each test, we create a new DiskManager
        disk_manager_ = std::make_unique<DiskManager>();
        // 如果测试目录不存在，则先创建测试目录
        if (!disk_manager_->is_dir(TEST_DB_NAME)) {
            disk_manager_->create_dir(TEST_DB_NAME);
        }
        assert(disk_manager_->is_dir(TEST_DB_NAME));
        // 进入测试目录
        if (chdir(TEST_DB_NAME.c_str()) < 0) {
            throw UnixError();
        }
        // 如果测试文件存在，则先删除原文件（最后留下来的文件存的是最后一个测试点的数据）
        if (disk_manager_->is_file(TEST_FILE_NAME)) {
            disk_manager_->destroy_file(TEST_FILE_NAME);
        }
        // 创建测试文件
        disk_manager_->create_file(TEST_FILE_NAME);
        assert(disk_manager_->is_file(TEST_FILE_NAME));
        // 打开测试文件
        fd_ = disk_manager_->open_file(TEST_FILE_NAME);
        assert(fd_ != -1);
    }

    // This function is called after every test.
    void TearDown() override {
        disk_manager_->close_file(fd_);
        // disk_manager_->destroy_file(TEST_FILE_NAME);  // you can choose to delete the file

        // 返回上一层目录
        if (chdir("..") < 0) {
            throw UnixError();
        }
        assert(disk_manager_->is_dir(TEST_DB_NAME));
    };
};

TEST_F(LeaderboardTest, Time) {
    // const size_t buffer_pool_size = 1000000; // 磁盘上写入3.9GB的文件
    const size_t buffer_pool_size = 100000;  // 磁盘上写入391MB的文件
    // create BufferPoolManager
    auto disk_manager = LeaderboardTest::disk_manager_.get();
    auto bpm = std::make_unique<BufferPoolManager>(buffer_pool_size, disk_manager);
    // create tmp PageId
    int fd = LeaderboardTest::fd_;
    PageId temp = {.fd = fd, .page_no = INVALID_PAGE_ID};

    for (size_t i = 0; i < buffer_pool_size; i++) {
        bpm->NewPage(&temp);
        bpm->FlushPage(PageId{fd, static_cast<page_id_t>(i)});  // 写入到磁盘（这句话可以注释掉）
    }
    for (size_t i = 0; i < buffer_pool_size; i++) {
        bpm->UnpinPage(PageId{fd, static_cast<page_id_t>(i)}, false);
        bpm->FetchPage(PageId{fd, static_cast<page_id_t>(i)});
        bpm->UnpinPage(PageId{fd, static_cast<page_id_t>(i)}, false);
    }
    for (size_t i = buffer_pool_size - 1; i != 0; i--) {
        bpm->DeletePage(PageId{fd, static_cast<page_id_t>(i)});
        bpm->NewPage(&temp);
        bpm->UnpinPage(temp, false);
        bpm->DeletePage(temp);
        bpm->NewPage(&temp);
    }
}
