//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// buffer_pool_manager_test.cpp
//
// Identification: test/buffer/buffer_pool_manager_test.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//
//                         Rucbase
//
// buffer_pool_manager_test.cpp
//
// Identification: src/storage/buffer_pool_manager_test.cpp
//
// Copyright (c) 2022, RUC Deke Group
//
//===----------------------------------------------------------------------===//

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <random>
#include <string>
#include <thread>  // NOLINT
#include <vector>

#include "buffer_pool_manager.h"
#include "gtest/gtest.h"

const std::string TEST_DB_NAME = "BufferPoolManagerTest_db";  // 以数据库名作为根目录
const std::string TEST_FILE_NAME = "basic";                   // 测试文件的名字

/** 注意：每个测试点只测试了单个文件！
 * 对于每个测试点，先创建和进入目录TEST_DB_NAME
 * 然后在此目录下创建和打开文件TEST_FILE_NAME，记录其文件描述符fd */

// Add by jiawen
class BufferPoolManagerTest : public ::testing::Test {
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

// NOLINTNEXTLINE
TEST_F(BufferPoolManagerTest, SampleTest) {
    // create BufferPoolManager
    const size_t buffer_pool_size = 10;
    auto disk_manager = BufferPoolManagerTest::disk_manager_.get();
    auto bpm = std::make_unique<BufferPoolManager>(buffer_pool_size, disk_manager);
    // create tmp PageId
    int fd = BufferPoolManagerTest::fd_;
    PageId page_id_temp = {.fd = fd, .page_no = INVALID_PAGE_ID};
    auto *page0 = bpm->NewPage(&page_id_temp);

    // Scenario: The buffer pool is empty. We should be able to create a new page.
    ASSERT_NE(nullptr, page0);
    EXPECT_EQ(0, page_id_temp.page_no);

    // Scenario: Once we have a page, we should be able to read and write content.
    snprintf(page0->GetData(), sizeof(page0->GetData()), "Hello");
    EXPECT_EQ(0, strcmp(page0->GetData(), "Hello"));

    // Scenario: We should be able to create new pages until we fill up the buffer pool.
    for (size_t i = 1; i < buffer_pool_size; ++i) {
        EXPECT_NE(nullptr, bpm->NewPage(&page_id_temp));
    }

    // Scenario: Once the buffer pool is full, we should not be able to create any new pages.
    for (size_t i = buffer_pool_size; i < buffer_pool_size * 2; ++i) {
        EXPECT_EQ(nullptr, bpm->NewPage(&page_id_temp));
    }

    // Scenario: After unpinning pages {0, 1, 2, 3, 4} and pinning another 4 new pages,
    // there would still be one cache frame left for reading page 0.
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(true, bpm->UnpinPage(PageId{fd, i}, true));
    }
    for (int i = 0; i < 4; ++i) {
        EXPECT_NE(nullptr, bpm->NewPage(&page_id_temp));
    }

    // Scenario: We should be able to fetch the data we wrote a while ago.
    page0 = bpm->FetchPage(PageId{fd, 0});
    EXPECT_EQ(0, strcmp(page0->GetData(), "Hello"));
    EXPECT_EQ(true, bpm->UnpinPage(PageId{fd, 0}, true));
    // NewPage again, and now all buffers are pinned. Page 0 would be failed to fetch.
    EXPECT_NE(nullptr, bpm->NewPage(&page_id_temp));
    EXPECT_EQ(nullptr, bpm->FetchPage(PageId{fd, 0}));

    bpm->FlushAllPages(fd);
}

TEST_F(BufferPoolManagerTest, BinaryDataTest) {
    // create BufferPoolManager
    const size_t buffer_pool_size = 10;
    auto disk_manager = BufferPoolManagerTest::disk_manager_.get();
    auto bpm = std::make_unique<BufferPoolManager>(buffer_pool_size, disk_manager);
    // create tmp PageId
    int fd = BufferPoolManagerTest::fd_;
    PageId page_id_temp = {.fd = fd, .page_no = INVALID_PAGE_ID};

    auto *page0 = bpm->NewPage(&page_id_temp);

    // Scenario: The buffer pool is empty. We should be able to create a new page.
    ASSERT_NE(nullptr, page0);
    EXPECT_EQ(0, page_id_temp.page_no);

    char random_binary_data[PAGE_SIZE];
    unsigned int seed = 15645;
    for (char &i : random_binary_data) {
        i = static_cast<char>(rand_r(&seed) % 256);
    }

    random_binary_data[PAGE_SIZE / 2] = '\0';
    random_binary_data[PAGE_SIZE - 1] = '\0';

    // Scenario: Once we have a page, we should be able to read and write content.
    std::strncpy(page0->GetData(), random_binary_data, PAGE_SIZE);
    EXPECT_EQ(0, std::strcmp(page0->GetData(), random_binary_data));

    // Scenario: We should be able to create new pages until we fill up the buffer pool.
    for (size_t i = 1; i < buffer_pool_size; ++i) {
        EXPECT_NE(nullptr, bpm->NewPage(&page_id_temp));
    }

    // Scenario: Once the buffer pool is full, we should not be able to create any new pages.
    for (size_t i = buffer_pool_size; i < buffer_pool_size * 2; ++i) {
        EXPECT_EQ(nullptr, bpm->NewPage(&page_id_temp));
    }

    // Scenario: After unpinning pages {0, 1, 2, 3, 4} and pinning another 4 new pages,
    // there would still be one cache frame left for reading page 0.
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(true, bpm->UnpinPage(PageId{fd, i}, true));
        bpm->FlushPage(PageId{fd, i});
    }
    for (int i = 0; i < 5; ++i) {
        EXPECT_NE(nullptr, bpm->NewPage(&page_id_temp));
        bpm->UnpinPage(page_id_temp, false);
    }
    // Scenario: We should be able to fetch the data we wrote a while ago.
    page0 = bpm->FetchPage(PageId{fd, 0});
    EXPECT_EQ(0, strcmp(page0->GetData(), random_binary_data));
    EXPECT_EQ(true, bpm->UnpinPage(PageId{fd, 0}, true));

    bpm->FlushAllPages(fd);
}

TEST_F(BufferPoolManagerTest, NewPage) {
    // create BufferPoolManager
    const size_t buffer_pool_size = 10;
    auto disk_manager = BufferPoolManagerTest::disk_manager_.get();
    auto bpm = std::make_unique<BufferPoolManager>(buffer_pool_size, disk_manager);
    // create tmp PageId
    int fd = BufferPoolManagerTest::fd_;
    PageId temp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};

    std::vector<PageId> page_ids;

    for (int i = 0; i < 10; ++i) {
        auto new_page = bpm->NewPage(&temp_page_id);
        EXPECT_NE(nullptr, new_page);
        ASSERT_NE(nullptr, new_page);
        strcpy(new_page->GetData(), std::to_string(i).c_str());  // NOLINT
        page_ids.push_back(temp_page_id);
    }

    // all the pages are pinned, the buffer pool is full
    for (int i = 0; i < 100; ++i) {
        auto new_page = bpm->NewPage(&temp_page_id);
        EXPECT_EQ(nullptr, new_page);
    }

    // upin the first five pages, add them to LRU list, set as dirty
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(true, bpm->UnpinPage(page_ids[i], true));
    }

    // we have 5 empty slots in LRU list, evict page zero out of buffer pool
    for (int i = 0; i < 5; ++i) {
        auto new_page = bpm->NewPage(&temp_page_id);
        EXPECT_NE(nullptr, new_page);
        ASSERT_NE(nullptr, new_page);
        page_ids[i] = temp_page_id;
    }

    // all the pages are pinned, the buffer pool is full
    for (int i = 0; i < 100; ++i) {
        auto new_page = bpm->NewPage(&temp_page_id);
        EXPECT_EQ(nullptr, new_page);
    }

    // upin the first five pages, add them to LRU list
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(true, bpm->UnpinPage(page_ids[i], false));
    }

    // we have 5 empty slots in LRU list, evict page zero out of buffer pool
    for (int i = 0; i < 5; ++i) {
        EXPECT_NE(nullptr, bpm->NewPage(&temp_page_id));
    }

    // all the pages are pinned, the buffer pool is full
    for (int i = 0; i < 100; ++i) {
        auto new_page = bpm->NewPage(&temp_page_id);
        EXPECT_EQ(nullptr, new_page);
    }

    bpm->FlushAllPages(fd);
}

TEST_F(BufferPoolManagerTest, UnpinPage) {
    // create BufferPoolManager
    const size_t buffer_pool_size = 2;
    auto disk_manager = BufferPoolManagerTest::disk_manager_.get();
    auto bpm = std::make_unique<BufferPoolManager>(buffer_pool_size, disk_manager);
    // get fd
    int fd = BufferPoolManagerTest::fd_;

    PageId pageid0 = {.fd = fd, .page_no = INVALID_PAGE_ID};
    auto page0 = bpm->NewPage(&pageid0);
    ASSERT_NE(nullptr, page0);
    strcpy(page0->GetData(), "page0");  // NOLINT

    PageId pageid1 = {.fd = fd, .page_no = INVALID_PAGE_ID};
    auto page1 = bpm->NewPage(&pageid1);
    ASSERT_NE(nullptr, page1);
    strcpy(page1->GetData(), "page1");  // NOLINT

    EXPECT_EQ(1, bpm->UnpinPage(pageid0, true));
    EXPECT_EQ(1, bpm->UnpinPage(pageid1, true));

    PageId temp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};

    for (size_t i = 0; i < buffer_pool_size; i++) {
        auto new_page = bpm->NewPage(&temp_page_id);
        ASSERT_NE(nullptr, new_page);
        bpm->UnpinPage(temp_page_id, true);
    }

    auto page = bpm->FetchPage(pageid0);
    EXPECT_EQ(0, strcmp(page->GetData(), "page0"));
    strcpy(page->GetData(), "page0updated");  // NOLINT

    page = bpm->FetchPage(pageid1);
    EXPECT_EQ(0, strcmp(page->GetData(), "page1"));
    strcpy(page->GetData(), "page1updated");  // NOLINT

    EXPECT_EQ(1, bpm->UnpinPage(pageid0, false));
    EXPECT_EQ(1, bpm->UnpinPage(pageid1, true));

    for (size_t i = 0; i < buffer_pool_size; i++) {
        auto new_page = bpm->NewPage(&temp_page_id);
        ASSERT_NE(nullptr, new_page);
        bpm->UnpinPage(temp_page_id, true);
    }

    page = bpm->FetchPage(pageid0);
    EXPECT_EQ(0, strcmp(page->GetData(), "page0"));
    strcpy(page->GetData(), "page0updated");  // NOLINT

    page = bpm->FetchPage(pageid1);
    EXPECT_EQ(0, strcmp(page->GetData(), "page1updated"));
    strcpy(page->GetData(), "page1againupdated");  // NOLINT

    bpm->FlushAllPages(fd);
}

TEST_F(BufferPoolManagerTest, FetchPage) {
    // create BufferPoolManager
    const size_t buffer_pool_size = 10;
    auto disk_manager = BufferPoolManagerTest::disk_manager_.get();
    auto bpm = std::make_unique<BufferPoolManager>(buffer_pool_size, disk_manager);
    // create tmp PageId
    int fd = BufferPoolManagerTest::fd_;
    PageId temp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};

    std::vector<Page *> pages;
    std::vector<PageId> page_ids;
    std::vector<std::string> content;

    for (size_t i = 0; i < buffer_pool_size; ++i) {
        auto new_page = bpm->NewPage(&temp_page_id);
        ASSERT_NE(nullptr, new_page);
        strcpy(new_page->GetData(), std::to_string(i).c_str());  // NOLINT
        pages.push_back(new_page);
        page_ids.push_back(temp_page_id);
        content.push_back(std::to_string(i));
    }

    for (size_t i = 0; i < buffer_pool_size; ++i) {
        auto page = bpm->FetchPage(page_ids[i]);
        ASSERT_NE(nullptr, page);
        EXPECT_EQ(pages[i], page);
        EXPECT_EQ(0, std::strcmp(std::to_string(i).c_str(), (page->GetData())));
        EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], true));
        EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], true));
        bpm->FlushPage(page_ids[i]);
    }

    for (size_t i = 0; i < buffer_pool_size; ++i) {
        auto new_page = bpm->NewPage(&temp_page_id);
        ASSERT_NE(nullptr, new_page);
        bpm->UnpinPage(temp_page_id, true);
    }

    for (size_t i = 0; i < buffer_pool_size; ++i) {
        auto page = bpm->FetchPage(page_ids[i]);
        ASSERT_NE(nullptr, page);
    }

    EXPECT_EQ(1, bpm->UnpinPage(page_ids[4], true));
    auto new_page = bpm->NewPage(&temp_page_id);
    ASSERT_NE(nullptr, new_page);
    EXPECT_EQ(nullptr, bpm->FetchPage(page_ids[4]));

    // Check Clock
    auto page5 = bpm->FetchPage(page_ids[5]);
    auto page6 = bpm->FetchPage(page_ids[6]);
    auto page7 = bpm->FetchPage(page_ids[7]);
    EXPECT_NE(nullptr, page5);
    EXPECT_NE(nullptr, page6);
    EXPECT_NE(nullptr, page7);
    strcpy(page5->GetData(), "updatedpage5");  // NOLINT
    strcpy(page6->GetData(), "updatedpage6");  // NOLINT
    strcpy(page7->GetData(), "updatedpage7");  // NOLINT
    EXPECT_EQ(1, bpm->UnpinPage(page_ids[5], false));
    EXPECT_EQ(1, bpm->UnpinPage(page_ids[6], false));
    EXPECT_EQ(1, bpm->UnpinPage(page_ids[7], false));

    EXPECT_EQ(1, bpm->UnpinPage(page_ids[5], false));
    EXPECT_EQ(1, bpm->UnpinPage(page_ids[6], false));
    EXPECT_EQ(1, bpm->UnpinPage(page_ids[7], false));

    // page5 would be evicted.
    new_page = bpm->NewPage(&temp_page_id);
    ASSERT_NE(nullptr, new_page);
    // page6 would be evicted.
    page5 = bpm->FetchPage(page_ids[5]);
    EXPECT_NE(nullptr, page5);
    EXPECT_EQ(0, std::strcmp("5", (page5->GetData())));
    page7 = bpm->FetchPage(page_ids[7]);
    EXPECT_NE(nullptr, page7);
    EXPECT_EQ(0, std::strcmp("updatedpage7", (page7->GetData())));
    // All pages pinned
    EXPECT_EQ(nullptr, bpm->FetchPage(page_ids[6]));
    bpm->UnpinPage(temp_page_id, false);
    page6 = bpm->FetchPage(page_ids[6]);
    EXPECT_NE(nullptr, page6);
    EXPECT_EQ(0, std::strcmp("6", page6->GetData()));

    strcpy(page6->GetData(), "updatedpage6");  // NOLINT

    // Remove from LRU and update pin_count on fetch
    new_page = bpm->NewPage(&temp_page_id);
    EXPECT_EQ(nullptr, new_page);

    EXPECT_EQ(1, bpm->UnpinPage(page_ids[7], false));
    EXPECT_EQ(1, bpm->UnpinPage(page_ids[6], false));

    new_page = bpm->NewPage(&temp_page_id);
    ASSERT_NE(nullptr, new_page);
    page6 = bpm->FetchPage(page_ids[6]);
    EXPECT_NE(nullptr, page6);
    EXPECT_EQ(0, std::strcmp("updatedpage6", page6->GetData()));
    page7 = bpm->FetchPage(page_ids[7]);
    EXPECT_EQ(nullptr, page7);
    bpm->UnpinPage(temp_page_id, false);
    page7 = bpm->FetchPage(page_ids[7]);
    EXPECT_NE(nullptr, page7);
    EXPECT_EQ(0, std::strcmp("7", (page7->GetData())));

    bpm->FlushAllPages(fd);
}

TEST_F(BufferPoolManagerTest, DeletePage) {
    // create BufferPoolManager
    const size_t buffer_pool_size = 10;
    auto disk_manager = BufferPoolManagerTest::disk_manager_.get();
    auto bpm = std::make_unique<BufferPoolManager>(buffer_pool_size, disk_manager);
    // create tmp PageId
    int fd = BufferPoolManagerTest::fd_;
    PageId temp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};

    std::vector<Page *> pages;
    std::vector<PageId> page_ids;
    std::vector<std::string> content;

    for (size_t i = 0; i < buffer_pool_size; ++i) {
        auto new_page = bpm->NewPage(&temp_page_id);
        ASSERT_NE(nullptr, new_page);
        strcpy(new_page->GetData(), std::to_string(i).c_str());  // NOLINT
        pages.push_back(new_page);
        page_ids.push_back(temp_page_id);
        content.push_back(std::to_string(i));
    }

    for (size_t i = 0; i < buffer_pool_size; ++i) {
        auto page = bpm->FetchPage(page_ids[i]);
        ASSERT_NE(nullptr, page);
        EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], true));
        EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], true));
    }

    for (size_t i = 0; i < buffer_pool_size; ++i) {
        auto new_page = bpm->NewPage(&temp_page_id);
        ASSERT_NE(nullptr, new_page);
        bpm->UnpinPage(temp_page_id, true);
    }

    for (size_t i = 0; i < buffer_pool_size; ++i) {
        auto page = bpm->FetchPage(page_ids[i]);
        ASSERT_NE(nullptr, page);
    }

    auto new_page = bpm->NewPage(&temp_page_id);
    EXPECT_EQ(nullptr, new_page);

    EXPECT_EQ(0, bpm->DeletePage(page_ids[4]));
    bpm->UnpinPage(PageId{fd, 4}, false);
    EXPECT_EQ(1, bpm->DeletePage(page_ids[4]));

    new_page = bpm->NewPage(&temp_page_id);
    EXPECT_NE(nullptr, new_page);
    ASSERT_NE(nullptr, new_page);

    auto page5 = bpm->FetchPage(page_ids[5]);
    ASSERT_NE(nullptr, page5);
    auto page6 = bpm->FetchPage(page_ids[6]);
    ASSERT_NE(nullptr, page6);
    auto page7 = bpm->FetchPage(page_ids[7]);
    ASSERT_NE(nullptr, page7);
    strcpy(page5->GetData(), "updatedpage5");  // NOLINT
    strcpy(page6->GetData(), "updatedpage6");  // NOLINT
    strcpy(page7->GetData(), "updatedpage7");  // NOLINT
    bpm->UnpinPage(PageId{fd, 5}, false);
    bpm->UnpinPage(PageId{fd, 6}, false);
    bpm->UnpinPage(PageId{fd, 7}, false);

    bpm->UnpinPage(PageId{fd, 5}, false);
    bpm->UnpinPage(PageId{fd, 6}, false);
    bpm->UnpinPage(PageId{fd, 7}, false);
    EXPECT_EQ(1, bpm->DeletePage(page_ids[7]));

    bpm->NewPage(&temp_page_id);
    page5 = bpm->FetchPage(page_ids[5]);
    page6 = bpm->FetchPage(page_ids[6]);
    ASSERT_NE(nullptr, page5);
    ASSERT_NE(nullptr, page6);
    EXPECT_EQ(0, std::strcmp(page5->GetData(), "updatedpage5"));
    EXPECT_EQ(0, std::strcmp(page6->GetData(), "updatedpage6"));

    bpm->FlushAllPages(fd);
}

TEST_F(BufferPoolManagerTest, IsDirty) {
    // create BufferPoolManager
    const size_t buffer_pool_size = 1;
    auto disk_manager = BufferPoolManagerTest::disk_manager_.get();
    auto bpm = std::make_unique<BufferPoolManager>(buffer_pool_size, disk_manager);
    // get fd
    int fd = BufferPoolManagerTest::fd_;

    // Make new page and write to it
    PageId pageid0 = {.fd = fd, .page_no = INVALID_PAGE_ID};
    auto page0 = bpm->NewPage(&pageid0);
    ASSERT_NE(nullptr, page0);
    EXPECT_EQ(0, page0->IsDirty());
    strcpy(page0->GetData(), "page0");  // NOLINT
    EXPECT_EQ(1, bpm->UnpinPage(pageid0, true));

    // Fetch again but don't write. Assert it is still marked as dirty
    page0 = bpm->FetchPage(pageid0);
    ASSERT_NE(nullptr, page0);
    EXPECT_EQ(1, page0->IsDirty());
    EXPECT_EQ(1, bpm->UnpinPage(pageid0, false));

    // Fetch and assert it is still dirty
    page0 = bpm->FetchPage(pageid0);
    ASSERT_NE(nullptr, page0);
    EXPECT_EQ(1, page0->IsDirty());
    EXPECT_EQ(1, bpm->UnpinPage(pageid0, false));

    // Create a new page, assert it's not dirty
    PageId pageid1 = {.fd = fd, .page_no = INVALID_PAGE_ID};
    auto page1 = bpm->NewPage(&pageid1);
    ASSERT_NE(nullptr, page1);
    EXPECT_EQ(0, page1->IsDirty());

    // Write to the page, and then delete it
    strcpy(page1->GetData(), "page1");  // NOLINT
    EXPECT_EQ(1, bpm->UnpinPage(pageid1, true));
    EXPECT_EQ(1, page1->IsDirty());
    EXPECT_EQ(1, bpm->DeletePage(pageid1));

    // Fetch page 0 again, and confirm its not dirty
    page0 = bpm->FetchPage(pageid0);
    ASSERT_NE(nullptr, page0);
    EXPECT_EQ(0, page0->IsDirty());

    bpm->FlushAllPages(fd);
}

TEST_F(BufferPoolManagerTest, IntegratedTest) {
    // create BufferPoolManager
    const size_t buffer_pool_size = 10;
    auto disk_manager = BufferPoolManagerTest::disk_manager_.get();
    auto bpm = std::make_unique<BufferPoolManager>(buffer_pool_size, disk_manager);
    // create tmp PageId
    int fd = BufferPoolManagerTest::fd_;
    PageId temp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};

    std::vector<PageId> page_ids;
    for (int j = 0; j < 1000; j++) {
        for (int i = 0; i < 10; i++) {
            auto new_page = bpm->NewPage(&temp_page_id);
            EXPECT_NE(nullptr, new_page);
            ASSERT_NE(nullptr, new_page);
            strcpy(new_page->GetData(), std::to_string(temp_page_id.page_no).c_str());  // NOLINT
            page_ids.push_back(temp_page_id);
        }
        for (unsigned int i = page_ids.size() - 10; i < page_ids.size(); i++) {
            EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], true));
        }
    }

    for (int j = 0; j < 10000; j++) {
        auto page = bpm->FetchPage(page_ids[j]);
        EXPECT_NE(nullptr, page);
        ASSERT_NE(nullptr, page);
        EXPECT_EQ(0, std::strcmp(std::to_string(page_ids[j].page_no).c_str(), page->GetData()));
        EXPECT_EQ(1, bpm->UnpinPage(page_ids[j], true));
        page_ids.push_back(temp_page_id);
    }
    for (int j = 0; j < 10000; j++) {
        EXPECT_EQ(1, bpm->DeletePage(page_ids[j]));
    }

    bpm->FlushAllPages(fd);
}
