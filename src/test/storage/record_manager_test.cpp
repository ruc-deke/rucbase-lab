// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/context.h"
#include "gtest/gtest.h"

// Keep the access-control shim from rewriting declarations in libstdc++ headers.
#define private public
#include "record/rm.h"
#undef private  // for use private variables in "rm.h"

int record_page_bytes(const RmFileHdr& file_hdr, int slot_count) {
    int bitmap_size = (slot_count + BITMAP_WIDTH - 1) / BITMAP_WIDTH;
    const int record_bytes = slot_count * file_hdr.record_size;
    return static_cast<int>(Page::OFFSET_PAGE_HDR) + static_cast<int>(sizeof(RmPageHdr)) + bitmap_size + record_bytes;
}

void rand_buf(int size, char* out_buf) {
    for (int i = 0; i < size; i++) {
        out_buf[i] = static_cast<char>(static_cast<unsigned>(rand()) & 0xFFU);
    }
}

struct rid_hash_t {
    /** @brief 使用两个无符号 32 位分量构造测试用 RID 哈希。 */
    size_t operator()(const Rid& rid) const {
        const uint64_t page_no = static_cast<uint32_t>(rid.page_no);
        const uint64_t slot_no = static_cast<uint32_t>(rid.slot_no);
        return std::hash<uint64_t>{}((page_no << 32U) | slot_no);
    }
};

struct rid_equal_t {
    bool operator()(const Rid& x, const Rid& y) const { return x.page_no == y.page_no && x.slot_no == y.slot_no; }
};

void check_equal(const RmFileHandle* file_handle,
                 const std::unordered_map<Rid, std::string, rid_hash_t, rid_equal_t>& mock) {
    Context context(nullptr, nullptr, nullptr);
    // Test all records
    for (auto& [rid, value] : mock) {
        const char* mock_buf = value.data();
        auto rec = file_handle->get_record(rid, &context);
        ASSERT_NE(rec, nullptr);
        ASSERT_EQ(memcmp(mock_buf, rec->data, file_handle->file_hdr_.record_size), 0);
    }
    // Randomly get record
    ASSERT_GT(file_handle->file_hdr_.num_pages, RM_FIRST_RECORD_PAGE);
    for (int i = 0; i < 10; i++) {
        Rid rid = {.page_no = 1 + rand() % (file_handle->file_hdr_.num_pages - 1),
                   .slot_no = rand() % file_handle->file_hdr_.num_records_per_page};
        bool mock_exist = mock.contains(rid);
        bool rm_exist = file_handle->is_record(rid);
        ASSERT_EQ(rm_exist, mock_exist);
    }
    // Test RM scan
    size_t num_records = 0;
    for (RmScan scan(file_handle); !scan.is_end(); scan.next()) {
        ASSERT_TRUE(mock.contains(scan.rid()));
        auto rec = file_handle->get_record(scan.rid(), &context);
        ASSERT_NE(rec, nullptr);
        ASSERT_EQ(memcmp(rec->data, mock.at(scan.rid()).data(), file_handle->file_hdr_.record_size), 0);
        num_records++;
    }
    ASSERT_EQ(num_records, mock.size());
}

// std::cout can call this, for example: std::cout << rid
std::ostream& operator<<(std::ostream& os, const Rid& rid) {
    return os << '(' << rid.page_no << ", " << rid.slot_no << ')';
}

/**
 * @brief 简单测试record的基本功能
 * @note lab1 计分：15 points
 */
TEST(RecordManagerTest, SimpleTest) {
    srand(20260812U);  // 固定种子，失败可以稳定复现

    Context context(nullptr, nullptr, nullptr);

    // 创建RmManager类的对象rm_manager
    auto disk_manager = std::make_unique<DiskManager>();
    auto buffer_pool_manager = std::make_unique<BufferPoolManager>(BUFFER_POOL_SIZE, disk_manager.get());
    auto rm_manager = std::make_unique<RmManager>(disk_manager.get(), buffer_pool_manager.get());

    std::unordered_map<Rid, std::string, rid_hash_t, rid_equal_t> mock;

    std::string filename = "abc.txt";

    int record_size = 4 + rand() % 256;  // 元组大小覆盖小记录到中等记录
    // test files
    {
        // 删除残留的同名文件
        if (disk_manager->is_file(filename)) {
            disk_manager->destroy_file(filename);
        }
        // 将file header写入到磁盘中的filename文件
        rm_manager->create_file(filename, record_size);
        // 将磁盘中的filename文件读出到内存中的file handle的file header
        std::unique_ptr<RmFileHandle> file_handle = rm_manager->open_file(filename);
        // 检查filename文件在内存中的file header的参数
        ASSERT_EQ(file_handle->file_hdr_.record_size, record_size);
        ASSERT_EQ(file_handle->file_hdr_.first_free_page_no, RM_NO_PAGE);
        ASSERT_EQ(file_handle->file_hdr_.num_pages, RM_FIRST_RECORD_PAGE);

        ASSERT_LE(record_page_bytes(file_handle->file_hdr_, file_handle->file_hdr_.num_records_per_page), PAGE_SIZE);
        ASSERT_GT(record_page_bytes(file_handle->file_hdr_, file_handle->file_hdr_.num_records_per_page + 1),
                  PAGE_SIZE);
        constexpr int persisted_page_count = 37;
        file_handle->file_hdr_.num_pages = persisted_page_count;
        rm_manager->close_file(file_handle.get());

        // reopen file
        file_handle = rm_manager->open_file(filename);
        ASSERT_EQ(file_handle->file_hdr_.num_pages, persisted_page_count);
        rm_manager->close_file(file_handle.get());
        rm_manager->destroy_file(filename);
    }
    // test pages
    rm_manager->create_file(filename, record_size);
    auto file_handle = rm_manager->open_file(filename);

    char write_buf[PAGE_SIZE];
    size_t add_cnt = 0;
    size_t upd_cnt = 0;
    size_t del_cnt = 0;
    for (int round = 0; round < 1000; round++) {
        double insert_prob = 1. - static_cast<double>(mock.size()) / 250.;
        double dice = rand() * 1. / RAND_MAX;
        if (mock.empty() || dice < insert_prob) {
            rand_buf(file_handle->file_hdr_.record_size, write_buf);
            Rid rid = file_handle->insert_record(write_buf, &context);
            mock[rid] = std::string(write_buf, file_handle->file_hdr_.record_size);
            add_cnt++;
            //            std::cout << "insert " << rid << '\n'; // operator<<(cout,rid)
        } else {
            // update or erase random rid
            size_t rid_idx = static_cast<size_t>(rand()) % mock.size();
            auto it = mock.begin();
            for (size_t i = 0; i < rid_idx; i++) {
                it++;
            }
            auto rid = it->first;
            if (rand() % 2 == 0) {
                // update
                rand_buf(file_handle->file_hdr_.record_size, write_buf);
                file_handle->update_record(rid, write_buf, &context);
                mock[rid] = std::string(write_buf, file_handle->file_hdr_.record_size);
                upd_cnt++;
                //                std::cout << "update " << rid << '\n';
            } else {
                // erase
                file_handle->delete_record(rid, &context);
                mock.erase(rid);
                del_cnt++;
                //                std::cout << "delete " << rid << '\n';
            }
        }
        // Randomly re-open file
        if (round % 50 == 0) {
            rm_manager->close_file(file_handle.get());
            file_handle = rm_manager->open_file(filename);
        }
        check_equal(file_handle.get(), mock);
    }
    ASSERT_EQ(mock.size(), add_cnt - del_cnt);
    std::cout << "insert " << add_cnt << '\n' << "delete " << del_cnt << '\n' << "update " << upd_cnt << '\n';
    // clean up
    rm_manager->close_file(file_handle.get());
    rm_manager->destroy_file(filename);
}

/**
 * @brief 多文件测试record
 * @note lab1 计分：15 points
 */
TEST(RecordManagerTest, MultipleFilesTest) {
    srand(20260813U);  // 与 SimpleTest 使用不同的固定序列

    // 创建RmManager类的对象rm_manager
    auto disk_manager = std::make_unique<DiskManager>();
    auto buffer_pool_manager = std::make_unique<BufferPoolManager>(BUFFER_POOL_SIZE, disk_manager.get());
    auto rm_manager = std::make_unique<RmManager>(disk_manager.get(), buffer_pool_manager.get());

    std::vector<std::string> filenames;
    constexpr int MAX_FILES = 32;

    for (int i = 0; i < MAX_FILES; i++) {
        std::string filename = std::to_string(i) + ".txt";
        filenames.push_back(filename);
    }

    for (int i = 0; i < MAX_FILES; i++) {
        const std::string& filename = filenames[i];

        int record_size = 4 + rand() % 256;  // 元组大小覆盖小记录到中等记录

        // 删除残留的同名文件
        if (disk_manager->is_file(filename)) {
            disk_manager->destroy_file(filename);
        }

        // 将file header写入到磁盘中的filename文件
        rm_manager->create_file(filename, record_size);
        // 将磁盘中的filename文件读出到内存中的file handle的file header
        std::unique_ptr<RmFileHandle> file_handle = rm_manager->open_file(filename);
        // 检查filename文件在内存中的file header的参数
        ASSERT_EQ(file_handle->file_hdr_.record_size, record_size);
        ASSERT_EQ(file_handle->file_hdr_.first_free_page_no, RM_NO_PAGE);
        // printf("file_handle->file_hdr_.num_pages=%d\n", file_handle->file_hdr_.num_pages);
        ASSERT_EQ(file_handle->file_hdr_.num_pages, RM_FIRST_RECORD_PAGE);

        ASSERT_LE(record_page_bytes(file_handle->file_hdr_, file_handle->file_hdr_.num_records_per_page), PAGE_SIZE);
        ASSERT_GT(record_page_bytes(file_handle->file_hdr_, file_handle->file_hdr_.num_records_per_page + 1),
                  PAGE_SIZE);
        const int persisted_page_count = 100 + i;
        file_handle->file_hdr_.num_pages = persisted_page_count;
        rm_manager->close_file(file_handle.get());

        // reopen file
        file_handle = rm_manager->open_file(filename);
        ASSERT_EQ(file_handle->file_hdr_.num_pages, persisted_page_count);
        rm_manager->close_file(file_handle.get());
    }

    for (int i = 0; i < MAX_FILES; i++) {
        const std::string& filename = filenames[i];
        rm_manager->destroy_file(filename);
    }
}
