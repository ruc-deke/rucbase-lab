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
const std::string TEST_FILE_NAME = "concurrency";             // 测试文件的名字

/** 注意：每个测试点只测试了单个文件！
 * 对于每个测试点，先创建和进入目录TEST_DB_NAME
 * 然后在此目录下创建和打开文件TEST_FILE_NAME，记录其文件描述符fd */

// Add by jiawen
class BufferPoolManagerConcurrencyTest : public ::testing::Test {
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

TEST_F(BufferPoolManagerConcurrencyTest, ConcurrencyTest) {
    const int num_threads = 5;
    const int num_runs = 50;

    // get fd
    int fd = BufferPoolManagerConcurrencyTest::fd_;

    for (int run = 0; run < num_runs; run++) {
        // create BufferPoolManager
        auto disk_manager = BufferPoolManagerConcurrencyTest::disk_manager_.get();
        std::shared_ptr<BufferPoolManager> bpm{new BufferPoolManager(50, disk_manager)};

        std::vector<std::thread> threads;
        for (int tid = 0; tid < num_threads; tid++) {
            threads.push_back(std::thread([&bpm, fd]() {  // NOLINT
                PageId temp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};
                std::vector<PageId> page_ids;
                for (int i = 0; i < 10; i++) {
                    auto new_page = bpm->NewPage(&temp_page_id);
                    EXPECT_NE(nullptr, new_page);
                    ASSERT_NE(nullptr, new_page);
                    strcpy(new_page->GetData(), std::to_string(temp_page_id.page_no).c_str());  // NOLINT
                    page_ids.push_back(temp_page_id);
                }
                for (int i = 0; i < 10; i++) {
                    EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], true));
                }
                for (int j = 0; j < 10; j++) {
                    auto page = bpm->FetchPage(page_ids[j]);
                    EXPECT_NE(nullptr, page);
                    ASSERT_NE(nullptr, page);
                    EXPECT_EQ(0, std::strcmp(std::to_string(page_ids[j].page_no).c_str(), (page->GetData())));
                    EXPECT_EQ(1, bpm->UnpinPage(page_ids[j], true));
                }
                for (int j = 0; j < 10; j++) {
                    EXPECT_EQ(1, bpm->DeletePage(page_ids[j]));
                }
                bpm->FlushAllPages(fd);  // add this test by jiawen
            }));
        }  // end loop tid=[0,num_threads)

        for (int i = 0; i < num_threads; i++) {
            threads[i].join();
        }
    }  // end loop run=[0,num_runs)
}

// NOT test Concurrency
TEST_F(BufferPoolManagerConcurrencyTest, HardTest_1) {
    // create BufferPoolManager
    auto disk_manager = BufferPoolManagerConcurrencyTest::disk_manager_.get();
    auto bpm = std::make_unique<BufferPoolManager>(10, disk_manager);
    // create tmp PageId
    int fd = BufferPoolManagerConcurrencyTest::fd_;
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
        for (unsigned int i = page_ids.size() - 10; i < page_ids.size() - 5; i++) {
            EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], false));
        }
        for (unsigned int i = page_ids.size() - 5; i < page_ids.size(); i++) {
            EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], true));
        }
    }

    for (int j = 0; j < 10000; j++) {
        auto page = bpm->FetchPage(page_ids[j]);
        EXPECT_NE(nullptr, page);
        ASSERT_NE(nullptr, page);
        if (j % 10 < 5) {
            EXPECT_NE(0, std::strcmp(std::to_string(page_ids[j].page_no).c_str(), (page->GetData())));
        } else {
            EXPECT_EQ(0, std::strcmp(std::to_string(page_ids[j].page_no).c_str(), (page->GetData())));
        }
        EXPECT_EQ(1, bpm->UnpinPage(page_ids[j], true));
    }

    auto rng = std::default_random_engine{};
    std::shuffle(page_ids.begin(), page_ids.end(), rng);  // C++17 std::shuffle

    for (int j = 0; j < 5000; j++) {
        auto page = bpm->FetchPage(page_ids[j]);
        EXPECT_NE(nullptr, page);
        ASSERT_NE(nullptr, page);
        EXPECT_EQ(1, bpm->UnpinPage(page_ids[j], false));
        EXPECT_EQ(1, bpm->DeletePage(page_ids[j]));
    }

    for (int j = 5000; j < 10000; j++) {
        auto page = bpm->FetchPage(page_ids[j]);
        EXPECT_NE(nullptr, page);
        ASSERT_NE(nullptr, page);
        if (page_ids[j].page_no % 10 < 5) {
            EXPECT_NE(0, std::strcmp(std::to_string(page_ids[j].page_no).c_str(), (page->GetData())));
        } else {
            EXPECT_EQ(0, std::strcmp(std::to_string(page_ids[j].page_no).c_str(), (page->GetData())));
        }
        EXPECT_EQ(1, bpm->UnpinPage(page_ids[j], false));
        EXPECT_EQ(1, bpm->DeletePage(page_ids[j]));
    }

    bpm->FlushAllPages(fd);  // add this test by jiawen
}

TEST_F(BufferPoolManagerConcurrencyTest, HardTest_2) {
    const int num_threads = 5;
    const int num_runs = 50;
    for (int run = 0; run < num_runs; run++) {
        // create BufferPoolManager
        auto disk_manager = BufferPoolManagerConcurrencyTest::disk_manager_.get();
        std::shared_ptr<BufferPoolManager> bpm{new BufferPoolManager(50, disk_manager)};

        // create tmp PageId
        int fd = BufferPoolManagerConcurrencyTest::fd_;
        PageId temp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};

        std::vector<PageId> page_ids;
        for (int i = 0; i < 50; i++) {
            auto *new_page = bpm->NewPage(&temp_page_id);
            EXPECT_NE(nullptr, new_page);
            ASSERT_NE(nullptr, new_page);
            strcpy(new_page->GetData(), std::to_string(temp_page_id.page_no).c_str());  // NOLINT
            page_ids.push_back(temp_page_id);
        }

        for (int i = 0; i < 50; i++) {
            if (i % 2 == 0) {
                EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], true));
            } else {
                EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], false));
            }
        }

        for (int i = 0; i < 50; i++) {
            auto *new_page = bpm->NewPage(&temp_page_id);
            EXPECT_NE(nullptr, new_page);
            ASSERT_NE(nullptr, new_page);
            EXPECT_EQ(1, bpm->UnpinPage(temp_page_id, true));
        }

        for (int j = 0; j < 50; j++) {
            auto *page = bpm->FetchPage(page_ids[j]);
            EXPECT_NE(nullptr, page);
            ASSERT_NE(nullptr, page);
            strcpy(page->GetData(), (std::string("Hard") + std::to_string(page_ids[j].page_no)).c_str());  // NOLINT
        }

        for (int i = 0; i < 50; i++) {
            if (i % 2 == 0) {
                EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], false));
            } else {
                EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], true));
            }
        }

        for (int i = 0; i < 50; i++) {
            auto *new_page = bpm->NewPage(&temp_page_id);
            EXPECT_NE(nullptr, new_page);
            ASSERT_NE(nullptr, new_page);
            EXPECT_EQ(1, bpm->UnpinPage(temp_page_id, true));
        }

        std::vector<std::thread> threads;
        for (int tid = 0; tid < num_threads; tid++) {
            threads.push_back(std::thread([&bpm, tid, page_ids, fd]() {  // NOLINT
                int j = (tid * 10);
                while (j < 50) {
                    auto *page = bpm->FetchPage(page_ids[j]);
                    while (page == nullptr) {
                        page = bpm->FetchPage(page_ids[j]);
                    }
                    EXPECT_NE(nullptr, page);
                    ASSERT_NE(nullptr, page);
                    if (j % 2 == 0) {
                        EXPECT_EQ(0, std::strcmp(std::to_string(page_ids[j].page_no).c_str(), (page->GetData())));
                        EXPECT_EQ(1, bpm->UnpinPage(page_ids[j], false));
                    } else {
                        EXPECT_EQ(0, std::strcmp((std::string("Hard") + std::to_string(page_ids[j].page_no)).c_str(),
                                                 (page->GetData())));
                        EXPECT_EQ(1, bpm->UnpinPage(page_ids[j], false));
                    }
                    j = (j + 1);
                }
                bpm->FlushAllPages(fd);  // add this test by jiawen
            }));
        }

        for (int i = 0; i < num_threads; i++) {
            threads[i].join();
        }

        for (int j = 0; j < 50; j++) {
            EXPECT_EQ(1, bpm->DeletePage(page_ids[j]));
        }
    }
}

TEST_F(BufferPoolManagerConcurrencyTest, HardTest_3) {
    const int num_threads = 5;
    const int num_runs = 50;

    // get fd
    int fd = BufferPoolManagerConcurrencyTest::fd_;

    for (int run = 0; run < num_runs; run++) {
        // create BufferPoolManager
        auto disk_manager = BufferPoolManagerConcurrencyTest::disk_manager_.get();
        std::shared_ptr<BufferPoolManager> bpm{new BufferPoolManager(50, disk_manager)};

        PageId temp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};
        std::vector<PageId> page_ids;
        for (int i = 0; i < 50; i++) {
            auto *new_page = bpm->NewPage(&temp_page_id);
            EXPECT_NE(nullptr, new_page);
            ASSERT_NE(nullptr, new_page);
            strcpy(new_page->GetData(), std::to_string(temp_page_id.page_no).c_str());  // NOLINT
            page_ids.push_back(temp_page_id);
        }

        for (int i = 0; i < 50; i++) {
            if (i % 2 == 0) {
                EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], true));
            } else {
                EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], false));
            }
        }

        for (int i = 0; i < 50; i++) {
            auto *new_page = bpm->NewPage(&temp_page_id);
            EXPECT_NE(nullptr, new_page);
            ASSERT_NE(nullptr, new_page);
            EXPECT_EQ(1, bpm->UnpinPage(temp_page_id, true));
        }

        for (int j = 0; j < 50; j++) {
            auto *page = bpm->FetchPage(page_ids[j]);
            EXPECT_NE(nullptr, page);
            ASSERT_NE(nullptr, page);
            strcpy(page->GetData(), (std::string("Hard") + std::to_string(page_ids[j].page_no)).c_str());  // NOLINT
        }

        for (int i = 0; i < 50; i++) {
            if (i % 2 == 0) {
                EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], false));
            } else {
                EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], true));
            }
        }

        for (int i = 0; i < 50; i++) {
            auto *new_page = bpm->NewPage(&temp_page_id);
            EXPECT_NE(nullptr, new_page);
            ASSERT_NE(nullptr, new_page);
            EXPECT_EQ(1, bpm->UnpinPage(temp_page_id, true));
        }

        std::vector<std::thread> threads;
        for (int tid = 0; tid < num_threads; tid++) {
            threads.push_back(std::thread([&bpm, tid, page_ids, fd]() {  // NOLINT
                PageId temp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};
                int j = (tid * 10);
                while (j < 50) {
                    if (j != tid * 10) {
                        auto *page_local = bpm->FetchPage(temp_page_id);
                        while (page_local == nullptr) {
                            page_local = bpm->FetchPage(temp_page_id);
                        }
                        EXPECT_NE(nullptr, page_local);
                        ASSERT_NE(nullptr, page_local);
                        EXPECT_EQ(0,
                                  std::strcmp(std::to_string(temp_page_id.page_no).c_str(), (page_local->GetData())));
                        EXPECT_EQ(1, bpm->UnpinPage(temp_page_id, false));
                        // If the page is still in buffer pool then put it in free list,
                        // else also we are happy
                        EXPECT_EQ(1, bpm->DeletePage(temp_page_id));
                    }

                    auto *page = bpm->FetchPage(page_ids[j]);
                    while (page == nullptr) {
                        page = bpm->FetchPage(page_ids[j]);
                    }
                    EXPECT_NE(nullptr, page);
                    ASSERT_NE(nullptr, page);
                    if (j % 2 == 0) {
                        EXPECT_EQ(0, std::strcmp(std::to_string(page_ids[j].page_no).c_str(), (page->GetData())));
                        EXPECT_EQ(1, bpm->UnpinPage(page_ids[j], false));
                    } else {
                        EXPECT_EQ(0, std::strcmp((std::string("Hard") + std::to_string(page_ids[j].page_no)).c_str(),
                                                 (page->GetData())));
                        EXPECT_EQ(1, bpm->UnpinPage(page_ids[j], false));
                    }
                    j = (j + 1);

                    page = bpm->NewPage(&temp_page_id);
                    while (page == nullptr) {
                        page = bpm->NewPage(&temp_page_id);
                    }
                    EXPECT_NE(nullptr, page);
                    ASSERT_NE(nullptr, page);
                    strcpy(page->GetData(), std::to_string(temp_page_id.page_no).c_str());  // NOLINT
                    EXPECT_EQ(1, bpm->UnpinPage(temp_page_id, true));
                }
                bpm->FlushAllPages(fd);  // add this test by jiawen
            }));
        }

        for (int i = 0; i < num_threads; i++) {
            threads[i].join();
        }

        for (int j = 0; j < 50; j++) {
            EXPECT_EQ(1, bpm->DeletePage(page_ids[j]));
        }
    }
}

TEST_F(BufferPoolManagerConcurrencyTest, HardTest_4) {
    const int num_threads = 5;
    const int num_runs = 50;

    // get fd
    int fd = BufferPoolManagerConcurrencyTest::fd_;

    for (int run = 0; run < num_runs; run++) {
        // create BufferPoolManager
        auto disk_manager = BufferPoolManagerConcurrencyTest::disk_manager_.get();
        std::shared_ptr<BufferPoolManager> bpm{new BufferPoolManager(50, disk_manager)};

        PageId temp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};
        std::vector<PageId> page_ids;
        for (int i = 0; i < 50; i++) {
            auto *new_page = bpm->NewPage(&temp_page_id);
            EXPECT_NE(nullptr, new_page);
            ASSERT_NE(nullptr, new_page);
            strcpy(new_page->GetData(), std::to_string(temp_page_id.page_no).c_str());  // NOLINT
            page_ids.push_back(temp_page_id);
        }

        for (int i = 0; i < 50; i++) {
            if (i % 2 == 0) {
                EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], true));
            } else {
                EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], false));
            }
        }

        for (int i = 0; i < 50; i++) {
            auto *new_page = bpm->NewPage(&temp_page_id);
            EXPECT_NE(nullptr, new_page);
            ASSERT_NE(nullptr, new_page);
            EXPECT_EQ(1, bpm->UnpinPage(temp_page_id, true));
        }

        for (int j = 0; j < 50; j++) {
            auto *page = bpm->FetchPage(page_ids[j]);
            EXPECT_NE(nullptr, page);
            ASSERT_NE(nullptr, page);
            strcpy(page->GetData(), (std::string("Hard") + std::to_string(page_ids[j].page_no)).c_str());  // NOLINT
        }

        for (int i = 0; i < 50; i++) {
            if (i % 2 == 0) {
                EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], false));
            } else {
                EXPECT_EQ(1, bpm->UnpinPage(page_ids[i], true));
            }
        }

        for (int i = 0; i < 50; i++) {
            auto *new_page = bpm->NewPage(&temp_page_id);
            EXPECT_NE(nullptr, new_page);
            ASSERT_NE(nullptr, new_page);
            EXPECT_EQ(1, bpm->UnpinPage(temp_page_id, true));
        }

        std::vector<std::thread> threads;
        for (int tid = 0; tid < num_threads; tid++) {
            threads.push_back(std::thread([&bpm, tid, page_ids, fd]() {  // NOLINT
                PageId temp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};
                int j = (tid * 10);
                while (j < 50) {
                    if (j != tid * 10) {
                        auto *page_local = bpm->FetchPage(temp_page_id);
                        while (page_local == nullptr) {
                            page_local = bpm->FetchPage(temp_page_id);
                        }
                        EXPECT_NE(nullptr, page_local);
                        ASSERT_NE(nullptr, page_local);
                        EXPECT_EQ(0,
                                  std::strcmp(std::to_string(temp_page_id.page_no).c_str(), (page_local->GetData())));
                        EXPECT_EQ(1, bpm->UnpinPage(temp_page_id, false));
                        // If the page is still in buffer pool then put it in free list,
                        // else also we are happy
                        EXPECT_EQ(1, bpm->DeletePage(temp_page_id));
                    }

                    auto *page = bpm->FetchPage(page_ids[j]);
                    while (page == nullptr) {
                        page = bpm->FetchPage(page_ids[j]);
                    }
                    EXPECT_NE(nullptr, page);
                    ASSERT_NE(nullptr, page);
                    if (j % 2 == 0) {
                        EXPECT_EQ(0, std::strcmp(std::to_string(page_ids[j].page_no).c_str(), (page->GetData())));
                        EXPECT_EQ(1, bpm->UnpinPage(page_ids[j], false));
                    } else {
                        EXPECT_EQ(0, std::strcmp((std::string("Hard") + std::to_string(page_ids[j].page_no)).c_str(),
                                                 (page->GetData())));
                        EXPECT_EQ(1, bpm->UnpinPage(page_ids[j], false));
                    }
                    j = (j + 1);

                    page = bpm->NewPage(&temp_page_id);
                    while (page == nullptr) {
                        page = bpm->NewPage(&temp_page_id);
                    }
                    EXPECT_NE(nullptr, page);
                    ASSERT_NE(nullptr, page);
                    strcpy(page->GetData(), std::to_string(temp_page_id.page_no).c_str());  // NOLINT
                    // FLush page instead of unpining with true
                    EXPECT_EQ(1, bpm->FlushPage(temp_page_id));
                    EXPECT_EQ(1, bpm->UnpinPage(temp_page_id, false));

                    // Flood with new pages
                    for (int k = 0; k < 10; k++) {
                        PageId flood_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};
                        auto *flood_page = bpm->NewPage(&flood_page_id);
                        while (flood_page == nullptr) {
                            flood_page = bpm->NewPage(&flood_page_id);
                        }
                        EXPECT_NE(nullptr, flood_page);
                        ASSERT_NE(nullptr, flood_page);
                        EXPECT_EQ(1, bpm->UnpinPage(flood_page_id, false));
                        // If the page is still in buffer pool then put it in free list,
                        // else also we are happy
                        EXPECT_EQ(1, bpm->DeletePage(flood_page_id));
                    }
                }
                // bpm->FlushAllPages(fd);  // add this test by jiawen
            }));
        }

        for (int i = 0; i < num_threads; i++) {
            threads[i].join();
        }

        for (int j = 0; j < 50; j++) {
            EXPECT_EQ(1, bpm->DeletePage(page_ids[j]));
        }
    }
}
