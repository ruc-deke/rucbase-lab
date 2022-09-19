//===----------------------------------------------------------------------===//
//
//                         Rucbase
//
// storage_gtest.cpp
//
// Identification: src/storage/storage_gtest.cpp
//
// Copyright (c) 2022, RUC Deke Group
//
//===----------------------------------------------------------------------===//

#undef NDEBUG

#define private public

#include "storage/buffer_pool_manager.h"

#undef private

#include <cassert>
#include <cstring>
#include <ctime>
#include <string>
#include <unordered_map>
#include <vector>

#include "gtest/gtest.h"

constexpr int MAX_FILES = 32;
constexpr int MAX_PAGES = 128;
constexpr size_t TEST_BUFFER_POOL_SIZE = MAX_FILES * MAX_PAGES;

// 创建BufferPoolManager
auto disk_manager = std::make_unique<DiskManager>();
auto buffer_pool_manager = std::make_unique<BufferPoolManager>(TEST_BUFFER_POOL_SIZE, disk_manager.get());

std::unordered_map<int, char *> mock;  // fd -> buffer

char *mock_get_page(int fd, int page_no) { return &mock[fd][page_no * PAGE_SIZE]; }

void check_disk(int fd, int page_no) {
    char buf[PAGE_SIZE];
    disk_manager->read_page(fd, page_no, buf, PAGE_SIZE);
    char *mock_buf = mock_get_page(fd, page_no);
    assert(memcmp(buf, mock_buf, PAGE_SIZE) == 0);
}

void check_disk_all() {
    for (auto &file : mock) {
        int fd = file.first;
        for (int page_no = 0; page_no < MAX_PAGES; page_no++) {
            check_disk(fd, page_no);
        }
    }
}

void check_cache(int fd, int page_no) {
    Page *page = buffer_pool_manager->FetchPage(PageId{fd, page_no});
    char *mock_buf = mock_get_page(fd, page_no);  // &mock[fd][page_no * PAGE_SIZE];
    assert(memcmp(page->GetData(), mock_buf, PAGE_SIZE) == 0);
    buffer_pool_manager->UnpinPage(PageId{fd, page_no}, false);
}

void check_cache_all() {
    for (auto &file : mock) {
        int fd = file.first;
        for (int page_no = 0; page_no < MAX_PAGES; page_no++) {
            check_cache(fd, page_no);
        }
    }
}

void rand_buf(int size, char *buf) {
    for (int i = 0; i < size; i++) {
        int rand_ch = rand() & 0xff;
        buf[i] = rand_ch;
    }
}

int rand_fd() {
    assert(mock.size() == MAX_FILES);
    int fd_idx = rand() % MAX_FILES;
    auto it = mock.begin();
    for (int i = 0; i < fd_idx; i++) {
        it++;
    }
    return it->first;
}

// TODO: fix detected memory leaks found by Google Test
TEST(StorageTest, SimpleTest) {
    srand((unsigned)time(nullptr));

    /** Test disk_manager */
    std::vector<std::string> filenames(MAX_FILES);  // MAX_FILES=32
    std::unordered_map<int, std::string> fd2name;
    for (size_t i = 0; i < filenames.size(); i++) {
        auto &filename = filenames[i];
        filename = std::to_string(i) + ".txt";
        if (disk_manager->is_file(filename)) {
            disk_manager->destroy_file(filename);
        }
        // open without create
        try {
            disk_manager->open_file(filename);
            assert(false);
        } catch (const FileNotFoundError &e) {
        }

        disk_manager->create_file(filename);
        assert(disk_manager->is_file(filename));
        try {
            disk_manager->create_file(filename);
            assert(false);
        } catch (const FileExistsError &e) {
        }

        // open file
        int fd = disk_manager->open_file(filename);
        char *tmp = new char[PAGE_SIZE * MAX_PAGES];  // TODO: fix error in detected memory leaks

        mock[fd] = tmp;
        fd2name[fd] = filename;

        disk_manager->set_fd2pageno(fd, 0);  // diskmanager在fd对应的文件中从0开始分配page_no
    }

    /** Test buffer_pool_manager*/
    int num_pages = 0;
    char init_buf[PAGE_SIZE];
    for (auto &fh : mock) {
        int fd = fh.first;
        for (page_id_t i = 0; i < MAX_PAGES; i++) {
            rand_buf(PAGE_SIZE, init_buf);  // 将init_buf填充PAGE_SIZE个字节的随机数据

            PageId tmp_page_id = {.fd = fd, .page_no = INVALID_PAGE_ID};
            Page *page = buffer_pool_manager->NewPage(&tmp_page_id);
            int page_no = tmp_page_id.page_no;
            assert(page_no != INVALID_PAGE_ID);
            assert(page_no == i);

            memcpy(page->GetData(), init_buf, PAGE_SIZE);
            buffer_pool_manager->UnpinPage(PageId{fd, page_no}, true);

            char *mock_buf = mock_get_page(fd, page_no);  // &mock[fd][page_no * PAGE_SIZE]
            memcpy(mock_buf, init_buf, PAGE_SIZE);

            num_pages++;

            check_cache(fd, page_no);  // 调用了FetchPage, UnpinPage
        }
    }
    check_cache_all();

    assert(num_pages == TEST_BUFFER_POOL_SIZE);

    /** Test FlushAllPages() */
    // Flush and test disk
    for (auto &entry : fd2name) {
        int fd = entry.first;
        buffer_pool_manager->FlushAllPages(fd);
        for (int page_no = 0; page_no < MAX_PAGES; page_no++) {
            check_disk(fd, page_no);
        }
    }
    check_disk_all();

    for (int r = 0; r < 10000; r++) {
        int fd = rand_fd();
        int page_no = rand() % MAX_PAGES;
        // fetch page
        Page *page = buffer_pool_manager->FetchPage(PageId{fd, page_no});
        char *mock_buf = mock_get_page(fd, page_no);
        assert(memcmp(page->GetData(), mock_buf, PAGE_SIZE) == 0);

        // modify
        rand_buf(PAGE_SIZE, init_buf);
        memcpy(page->GetData(), init_buf, PAGE_SIZE);
        memcpy(mock_buf, init_buf, PAGE_SIZE);

        buffer_pool_manager->UnpinPage(page->GetPageId(), true);

        // flush
        if (rand() % 10 == 0) {
            buffer_pool_manager->FlushPage(page->GetPageId());
            check_disk(fd, page_no);
        }
        // flush entire file
        if (rand() % 100 == 0) {
            buffer_pool_manager->FlushAllPages(fd);
        }
        // re-open file
        if (rand() % 100 == 0) {
            disk_manager->close_file(fd);
            auto filename = fd2name[fd];
            char *buf = mock[fd];
            fd2name.erase(fd);
            mock.erase(fd);
            int new_fd = disk_manager->open_file(filename);
            mock[new_fd] = buf;
            fd2name[new_fd] = filename;
        }
        // assert equal in cache
        check_cache(fd, page_no);
    }
    check_cache_all();

    for (auto &entry : fd2name) {
        int fd = entry.first;
        buffer_pool_manager->FlushAllPages(fd);
        for (int page_no = 0; page_no < MAX_PAGES; page_no++) {
            check_disk(fd, page_no);
        }
    }
    check_disk_all();

    // close and destroy files
    for (auto &entry : fd2name) {
        int fd = entry.first;
        auto &filename = entry.second;
        disk_manager->close_file(fd);
        disk_manager->destroy_file(filename);
        try {
            disk_manager->destroy_file(filename);
            assert(false);
        } catch (const FileNotFoundError &e) {
        }
    }
}