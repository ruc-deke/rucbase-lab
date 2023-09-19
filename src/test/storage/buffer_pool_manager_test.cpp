#include "storage/buffer_pool_manager.h"

#include <cassert>
#include <cstring>
#include <ctime>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "gtest/gtest.h"

constexpr int MAX_FILES = 32;
constexpr int MAX_PAGES = 128;
constexpr size_t TEST_BUFFER_POOL_SIZE = MAX_FILES * MAX_PAGES;
const std::string TEST_DB_NAME = "BufferPoolManagerTest_db";  // 以TEST_DB_NAME作为存放测试文件的根目录名

// Add by jiawen
class BufferPoolManagerTest : public ::testing::Test {
   public:
    std::unique_ptr<DiskManager> disk_manager_;

   public:
    // This function is called before every test.
    void SetUp() override {
        ::testing::Test::SetUp();
        // 对于每个测试点，创建一个disk manager
        disk_manager_ = std::make_unique<DiskManager>();
        // 如果测试目录存在，则先删除原目录
        if (disk_manager_->is_dir(TEST_DB_NAME)) {
            disk_manager_->destroy_dir(TEST_DB_NAME);
        }
        // 创建一个新的目录
        disk_manager_->create_dir(TEST_DB_NAME);
        assert(disk_manager_->is_dir(TEST_DB_NAME));  // 检查是否创建目录成功
        // 进入测试目录
        if (chdir(TEST_DB_NAME.c_str()) < 0) {
            throw UnixError();
        }
    }

    // This function is called after every test.
    void TearDown() override {
        // 返回上一层目录
        if (chdir("..") < 0) {
            throw UnixError();
        }
        assert(disk_manager_->is_dir(TEST_DB_NAME));
    };

    /**
     * @brief 将buf填充size个字节的随机数据
     */
    void rand_buf(char *buf, int size) {
        srand((unsigned)time(nullptr));
        for (int i = 0; i < size; i++) {
            int rand_ch = rand() & 0xff;
            buf[i] = rand_ch;
        }
    }

    /**
     * @brief 随机获取mock中的键
     */
    int rand_fd(std::unordered_map<int, char *> mock) {
        assert(mock.size() == MAX_FILES);
        int fd_idx = rand() % MAX_FILES;
        auto it = mock.begin();
        for (int i = 0; i < fd_idx; i++) {
            it++;
        }
        return it->first;
    }
};

/**
 * @brief 简单测试缓冲池的基本功能（单文件）
 * @note 生成测试文件simple_test
 * @note lab1 计分：5 points
 */
TEST_F(BufferPoolManagerTest, SimpleTest) {
    const std::string filename = "simple_test";

    // create BufferPoolManager
    const size_t buffer_pool_size = 10;
    auto disk_manager = BufferPoolManagerTest::disk_manager_.get();
    auto bpm = std::make_unique<BufferPoolManager>(buffer_pool_size, disk_manager);
    // create and open file
    disk_manager_->create_file(filename);
    int fd = disk_manager_->open_file(filename);
    // create tmp PageId
    PageId tmp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};
    auto *page0 = bpm->new_page(&tmp_page_id);

    // Scenario: The buffer pool is empty. We should be able to create a new page.
    ASSERT_NE(nullptr, page0);
    EXPECT_EQ(0, tmp_page_id.page_no);

    // Scenario: Once we have a page, we should be able to read and write content.
    snprintf(page0->get_data(), sizeof(page0->get_data()), "Hello");
    EXPECT_EQ(0, strcmp(page0->get_data(), "Hello"));

    // Scenario: We should be able to create new pages until we fill up the buffer pool.
    for (size_t i = 1; i < buffer_pool_size; ++i) {
        EXPECT_NE(nullptr, bpm->new_page(&tmp_page_id));
    }

    // Scenario: Once the buffer pool is full, we should not be able to create any new pages.
    for (size_t i = buffer_pool_size; i < buffer_pool_size * 2; ++i) {
        EXPECT_EQ(nullptr, bpm->new_page(&tmp_page_id));
    }

    // Scenario: After unpinning pages {0, 1, 2, 3, 4} and pinning another 4 new pages,
    // there would still be one cache frame left for reading page 0.
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(true, bpm->unpin_page(PageId{fd, i}, true));
    }
    for (int i = 0; i < 4; ++i) {
        EXPECT_NE(nullptr, bpm->new_page(&tmp_page_id));
    }

    // Scenario: We should be able to fetch the data we wrote a while ago.
    page0 = bpm->fetch_page(PageId{fd, 0});
    EXPECT_EQ(0, strcmp(page0->get_data(), "Hello"));
    EXPECT_EQ(true, bpm->unpin_page(PageId{fd, 0}, true));
    // new_page again, and now all buffers are pinned. Page 0 would be failed to fetch.
    EXPECT_NE(nullptr, bpm->new_page(&tmp_page_id));
    EXPECT_EQ(nullptr, bpm->fetch_page(PageId{fd, 0}));

    bpm->flush_all_pages(fd);

    disk_manager_->close_file(fd);
}

/**
 * @brief 在SimpleTest的基础上加大数据量（单文件），生成测试文件large_scale_test
 * @note lab1 计分：10 points
 */
TEST_F(BufferPoolManagerTest, LargeScaleTest) {
    const int scale = 10000;
    const std::string filename = "large_scale_test";
    // create BufferPoolManager
    const int buffer_pool_size = 10;
    auto disk_manager = BufferPoolManagerTest::disk_manager_.get();
    auto bpm = std::make_unique<BufferPoolManager>(static_cast<size_t>(buffer_pool_size), disk_manager);
    // create and open file
    disk_manager_->create_file(filename);
    int fd = disk_manager_->open_file(filename);
    // create tmp PageId
    PageId tmp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};

    std::vector<PageId> page_ids;
    for (int i = 0; i < scale / buffer_pool_size; i++) {
        for (int j = 0; j < buffer_pool_size; j++) {
            auto new_page = bpm->new_page(&tmp_page_id);
            EXPECT_NE(nullptr, new_page);
            strcpy(new_page->get_data(), std::to_string(tmp_page_id.page_no).c_str());
            page_ids.push_back(tmp_page_id);
        }
        for (unsigned int j = page_ids.size() - buffer_pool_size; j < page_ids.size(); j++) {
            EXPECT_EQ(true, bpm->unpin_page(page_ids[j], true));
        }
    }

    for (int i = 0; i < scale; i++) {
        auto page = bpm->fetch_page(page_ids[i]);
        EXPECT_NE(nullptr, page);
        EXPECT_EQ(0, std::strcmp(std::to_string(page_ids[i].page_no).c_str(), page->get_data()));
        EXPECT_EQ(true, bpm->unpin_page(page_ids[i], true));
        page_ids.push_back(tmp_page_id);
    }

    for (int i = 0; i < scale; i++) {
        EXPECT_EQ(true, bpm->delete_page(page_ids[i]));
    }

    disk_manager_->close_file(fd);
}

/**
 * @brief 多文件测试
 * @note 生成若干测试文件multiple_files_test_*
 * @note lab1 计分：10 points
 */
TEST_F(BufferPoolManagerTest, MultipleFilesTest) {
    const size_t buffer_size = MAX_FILES * MAX_PAGES / 2;
    auto buffer_pool_manager = std::make_unique<BufferPoolManager>(buffer_size, disk_manager_.get());

    // mock记录生成文件的(文件fd, page在内存中的首地址)
    // page在内存中的首地址是page在内存中的备份
    std::unordered_map<int, char *> mock;  // fd -> page address

    std::vector<std::string> filenames(MAX_FILES);  // MAX_FILES=32
    std::unordered_map<int, std::string> fd2name;
    for (size_t i = 0; i < filenames.size(); i++) {
        auto &filename = filenames[i];
        filename = "multiple_files_test_" + std::to_string(i);
        if (disk_manager_->is_file(filename)) {
            disk_manager_->destroy_file(filename);
        }
        // create and open file
        disk_manager_->create_file(filename);
        int fd = disk_manager_->open_file(filename);

        mock[fd] = new char[PAGE_SIZE * MAX_PAGES];  // 申请PAGE_SIZE * MAX_PAGES个字节的内存空间，mock[fd]记录其首地址
        fd2name[fd] = filename;

        disk_manager_->set_fd2pageno(fd, 0);  // 设置diskmanager在fd对应的文件中从0开始分配page_no
    }

    char buf[PAGE_SIZE] = {0};

    /** Test new_page(), unpin_page() */
    for (auto &fh : mock) {
        int fd = fh.first;
        for (page_id_t i = 0; i < MAX_PAGES; i++) {
            rand_buf(buf, PAGE_SIZE);  // 生成buf，将buf填充PAGE_SIZE个字节的随机数据

            PageId tmp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};
            Page *page = buffer_pool_manager->new_page(&tmp_page_id);  // pin the page
            int page_no = tmp_page_id.page_no;
            EXPECT_EQ(page_no, i);

            memcpy(page->get_data(), buf, PAGE_SIZE);          // buf -> page
            char *mock_buf = &mock[fd][page_no * PAGE_SIZE];  // get mock address in (fd,page_no)
            memcpy(mock_buf, buf, PAGE_SIZE);                 // buf -> mock

            // check cache: page data == mock data
            EXPECT_EQ(memcmp(page->get_data(), mock_buf, PAGE_SIZE), 0);

            bool unpin_flag = buffer_pool_manager->unpin_page(page->get_page_id(), true);  // unpin the page
            EXPECT_EQ(unpin_flag, true);
        }
    }

    /** Test flush_all_pages(), fetch_page(), unpin_page() */
    // Flush and test disk
    for (auto &entry : fd2name) {
        int fd = entry.first;
        buffer_pool_manager->flush_all_pages(fd);  // wirte all pages in fd file into disk
        for (int page_no = 0; page_no < MAX_PAGES; page_no++) {
            // check disk: disk data == mock data
            disk_manager_->read_page(fd, page_no, buf, PAGE_SIZE);  // read page from disk (disk -> buf)
            char *mock_buf = &mock[fd][page_no * PAGE_SIZE];        // get mock address in (fd,page_no)
            EXPECT_EQ(memcmp(buf, mock_buf, PAGE_SIZE), 0);
            // check disk: disk data == page data
            Page *page = buffer_pool_manager->fetch_page(PageId{fd, page_no});
            EXPECT_EQ(memcmp(buf, page->get_data(), PAGE_SIZE), 0);
            bool unpin_flag = buffer_pool_manager->unpin_page(page->get_page_id(), false);
            EXPECT_EQ(unpin_flag, true);
        }
    }

    for (int r = 0; r < 10000; r++) {
        int fd = rand_fd(mock);
        int page_no = rand() % MAX_PAGES;
        // fetch page
        Page *page = buffer_pool_manager->fetch_page(PageId{fd, page_no});
        char *mock_buf = &mock[fd][page_no * PAGE_SIZE];
        assert(memcmp(page->get_data(), mock_buf, PAGE_SIZE) == 0);

        // modify
        rand_buf(buf, PAGE_SIZE);
        memcpy(page->get_data(), buf, PAGE_SIZE);
        memcpy(mock_buf, buf, PAGE_SIZE);

        // flush
        if (rand() % 10 == 0) {
            buffer_pool_manager->flush_page(page->get_page_id());
            // check disk: disk data == mock data
            disk_manager_->read_page(fd, page_no, buf, PAGE_SIZE);  // read page from disk (disk -> buf)
            char *mock_buf = &mock[fd][page_no * PAGE_SIZE];        // get mock address in (fd,page_no)
            EXPECT_EQ(memcmp(buf, mock_buf, PAGE_SIZE), 0);
        }
        // check cache: page data == mock data
        EXPECT_EQ(memcmp(page->get_data(), mock_buf, PAGE_SIZE), 0);

        bool unpin_flag = buffer_pool_manager->unpin_page(page->get_page_id(), true);  // unpin the page
        EXPECT_EQ(unpin_flag, true);
    }

    // close and destroy files
    for (auto &entry : fd2name) {
        int fd = entry.first;
        disk_manager_->close_file(fd);
        // auto &filename = entry.second;
        // disk_manager_->destroy_file(filename);
    }
}

/**
 * @brief 缓冲池并发测试（单文件）
 * @note 生成测试文件concurrency_test
 * @note lab1 计分：15 points
 */
TEST_F(BufferPoolManagerTest, ConcurrencyTest) {
    const int num_threads = 5;
    const int num_runs = 50;

    const std::string filename = "concurrency_test";
    const int buffer_pool_size = 50;

    // get disk manager
    auto disk_manager = BufferPoolManagerTest::disk_manager_.get();
    // create and open file
    disk_manager_->create_file(filename);
    int fd = disk_manager_->open_file(filename);

    for (int run = 0; run < num_runs; run++) {
        // create BufferPoolManager
        std::shared_ptr<BufferPoolManager> bpm{
            new BufferPoolManager(static_cast<size_t>(buffer_pool_size), disk_manager)};

        PageId tmp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};
        std::vector<PageId> page_ids;
        for (int i = 0; i < buffer_pool_size; i++) {
            auto *new_page = bpm->new_page(&tmp_page_id);
            EXPECT_NE(nullptr, new_page);
            strcpy(new_page->get_data(), std::to_string(tmp_page_id.page_no).c_str());
            page_ids.push_back(tmp_page_id);
        }

        for (int i = 0; i < buffer_pool_size; i++) {
            if (i % 2 == 0) {
                EXPECT_EQ(true, bpm->unpin_page(page_ids[i], true));
            } else {
                EXPECT_EQ(true, bpm->unpin_page(page_ids[i], false));
            }
        }

        for (int i = 0; i < buffer_pool_size; i++) {
            auto *new_page = bpm->new_page(&tmp_page_id);
            EXPECT_NE(nullptr, new_page);
            EXPECT_EQ(true, bpm->unpin_page(tmp_page_id, true));
        }

        for (int j = 0; j < buffer_pool_size; j++) {
            auto *page = bpm->fetch_page(page_ids[j]);
            EXPECT_NE(nullptr, page);
            strcpy(page->get_data(), (std::string("Hard") + std::to_string(page_ids[j].page_no)).c_str());
        }

        for (int i = 0; i < buffer_pool_size; i++) {
            if (i % 2 == 0) {
                EXPECT_EQ(true, bpm->unpin_page(page_ids[i], false));
            } else {
                EXPECT_EQ(true, bpm->unpin_page(page_ids[i], true));
            }
        }

        for (int i = 0; i < buffer_pool_size; i++) {
            auto *new_page = bpm->new_page(&tmp_page_id);
            EXPECT_NE(nullptr, new_page);
            EXPECT_EQ(true, bpm->unpin_page(tmp_page_id, true));
        }

        std::vector<std::thread> threads;
        for (int tid = 0; tid < num_threads; tid++) {
            threads.push_back(std::thread([&bpm, tid, page_ids, fd]() {
                PageId temp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};
                int j = (tid * 10);
                while (j < buffer_pool_size) {
                    if (j != tid * 10) {
                        auto *page_local = bpm->fetch_page(temp_page_id);
                        while (page_local == nullptr) {
                            page_local = bpm->fetch_page(temp_page_id);
                        }
                        EXPECT_NE(nullptr, page_local);
                        EXPECT_EQ(0,
                                  std::strcmp(std::to_string(temp_page_id.page_no).c_str(), (page_local->get_data())));
                        EXPECT_EQ(true, bpm->unpin_page(temp_page_id, false));
                        // If the page is still in buffer pool then put it in free list,
                        // else also we are happy
                        EXPECT_EQ(true, bpm->delete_page(temp_page_id));
                    }

                    auto *page = bpm->fetch_page(page_ids[j]);
                    while (page == nullptr) {
                        page = bpm->fetch_page(page_ids[j]);
                    }
                    EXPECT_NE(nullptr, page);
                    if (j % 2 == 0) {
                        EXPECT_EQ(0, std::strcmp(std::to_string(page_ids[j].page_no).c_str(), (page->get_data())));
                        EXPECT_EQ(true, bpm->unpin_page(page_ids[j], false));
                    } else {
                        EXPECT_EQ(0, std::strcmp((std::string("Hard") + std::to_string(page_ids[j].page_no)).c_str(),
                                                 (page->get_data())));
                        EXPECT_EQ(true, bpm->unpin_page(page_ids[j], false));
                    }
                    j = (j + 1);

                    page = bpm->new_page(&temp_page_id);
                    while (page == nullptr) {
                        page = bpm->new_page(&temp_page_id);
                    }
                    EXPECT_NE(nullptr, page);
                    strcpy(page->get_data(), std::to_string(temp_page_id.page_no).c_str());
                    // FLush page instead of unpining with true
                    EXPECT_EQ(true, bpm->flush_page(temp_page_id));
                    EXPECT_EQ(true, bpm->unpin_page(temp_page_id, false));

                    // Flood with new pages
                    for (int k = 0; k < 10; k++) {
                        PageId flood_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};
                        auto *flood_page = bpm->new_page(&flood_page_id);
                        while (flood_page == nullptr) {
                            flood_page = bpm->new_page(&flood_page_id);
                        }
                        EXPECT_NE(nullptr, flood_page);
                        EXPECT_EQ(true, bpm->unpin_page(flood_page_id, false));
                        // If the page is still in buffer pool then put it in free list,
                        // else also we are happy
                        EXPECT_EQ(true, bpm->delete_page(flood_page_id));
                    }
                }
            }));
        }

        for (int i = 0; i < num_threads; i++) {
            threads[i].join();
        }

        for (int j = 0; j < buffer_pool_size; j++) {
            EXPECT_EQ(true, bpm->delete_page(page_ids[j]));
        }
    }

    disk_manager_->close_file(fd);
}
