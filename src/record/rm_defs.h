#pragma once

#include "common/macros.h"
#include "defs.h"
#include "storage/buffer_pool_manager.h"

constexpr int RM_NO_PAGE = -1;
constexpr int RM_FILE_HDR_PAGE = 0;
constexpr int RM_FIRST_RECORD_PAGE = 1;
constexpr int RM_MAX_RECORD_SIZE = 512;

// record file header（RmManager::create_file函数初始化，并写入磁盘文件中的第0页）
struct RmFileHdr {
    int record_size;  // 元组大小（长度不固定，由上层进行初始化）
    // std::atomic<page_id_t> num_pages;
    int num_pages;             // 文件中当前分配的page个数（初始化为1）
    int num_records_per_page;  // 每个page最多能存储的元组个数
    int first_free_page_no;    // 文件中当前第一个可用的page no（初始化为-1）
    int bitmap_size;           // bitmap大小
};

// record page header（RmFileHandle::create_page函数进行初始化）
struct RmPageHdr {
    int next_free_page_no;  // 当前page满了之后，下一个可用的page no（初始化为-1）
    int num_records;        // 当前page中当前分配的record个数（初始化为0）
};

// 类似于Tuple
struct RmRecord {
    char *data;  // data初始化分配size个字节的空间
    int size;    // size = RmFileHdr的record_size
    bool allocated_ = false;

    // DISALLOW_COPY(RmRecord);
    // RmRecord(const RmRecord &other) = delete;
    // RmRecord &operator=(const RmRecord &other) = delete;

    RmRecord() = default;

    RmRecord(const RmRecord &other) {
        size = other.size;
        data = new char[size];
        memcpy(data, other.data, size);
        allocated_ = true;
    };

    RmRecord &operator=(const RmRecord &other) {
        size = other.size;
        data = new char[size];
        memcpy(data, other.data, size);
        allocated_ = true;
        return *this;
    };

    RmRecord(int size_) {
        size = size_;
        data = new char[size_];
        allocated_ = true;
    }

    RmRecord(int size_, char *data_) {
        size = size_;
        data = new char[size_];
        memcpy(data, data_, size_);
        allocated_ = true;
    }

    void SetData(char *data_) {
        memcpy(data, data_, size);
    }

    void Deserialize(const char *data_) {
        size = *reinterpret_cast<const int *>(data_);
        delete[] data;
        data = new char[size];
        memcpy(data, data_ + sizeof(int), size);
    }

    ~RmRecord() {
        if(allocated_) {
            delete[] data;
        }
        allocated_ = false;
        data = nullptr;
    }
};
