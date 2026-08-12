// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cstring>

#include "common/errors.h"

constexpr int RM_NO_PAGE = -1;           ///< 无有效页面，用作空闲页链表的终止标记。
constexpr int RM_FILE_HDR_PAGE = 0;      ///< 记录文件头固定占用的页号。
constexpr int RM_FIRST_RECORD_PAGE = 1;  ///< 第一张记录数据页的页号。
constexpr int RM_MAX_RECORD_SIZE = 512;  ///< 教学版本支持的单条定长记录最大字节数。

/* 文件头，记录表数据文件的元信息，写入磁盘中文件的第0号页面 */
struct RmFileHdr {
    int record_size;           // 表中每条记录的大小，由于不包含变长字段，因此当前字段初始化后保持不变
    int num_pages;             // 文件中分配的页面个数，包含第0号文件头页
    int num_records_per_page;  // 每个数据页最多能存储的记录数
    int first_free_page_no;    // 空闲页链表头；RM_NO_PAGE 表示链表为空
    int bitmap_size;           // 每个数据页的 bitmap 字节数
};

/* 表数据文件中每个页面的页头，记录每个页面的元信息 */
struct RmPageHdr {
    int next_free_page_no;  // 本页位于空闲页链表中时的后继页号
    int num_records;        // 当前页面中已经存储的记录数
};

/* 表中的记录 */
struct RmRecord {
    char* data = nullptr;  // 记录的数据
    int size = 0;          // 记录的大小
    bool allocated_ = false;

    RmRecord() = default;

    RmRecord(const RmRecord& other) { assign_from(other); }

    RmRecord& operator=(const RmRecord& other) {
        if (this != &other) {
            char* new_data = copy_data(other);
            release();
            data = new_data;
            size = other.size;
            allocated_ = data != nullptr;
        }
        return *this;
    }

    RmRecord(RmRecord&& other) noexcept : data(other.data), size(other.size), allocated_(other.allocated_) {
        other.data = nullptr;
        other.size = 0;
        other.allocated_ = false;
    }

    RmRecord& operator=(RmRecord&& other) noexcept {
        if (this != &other) {
            release();
            data = other.data;
            size = other.size;
            allocated_ = other.allocated_;
            other.data = nullptr;
            other.size = 0;
            other.allocated_ = false;
        }
        return *this;
    }

    explicit RmRecord(const int size_) {
        if (size_ < 0) {
            throw InvalidRecordSizeError(size_);
        }
        size = size_;
        if (size > 0) {
            data = new char[size];
            allocated_ = true;
        }
    }

    RmRecord(const int size_, const char* data_) {
        if (size_ < 0) {
            throw InvalidRecordSizeError(size_);
        }
        size = size_;
        if (size > 0) {
            if (data_ == nullptr) {
                throw InternalError("RmRecord: null source data");
            }
            data = new char[size];
            allocated_ = true;
            memcpy(data, data_, size);
        }
    }

    void SetData(const char* data_) {
        if (size > 0 && data_ == nullptr) {
            throw InternalError("RmRecord: null source data");
        }
        if (size > 0) {
            memcpy(data, data_, size);
        }
    }

    void Deserialize(const char* data_) {
        if (data_ == nullptr) {
            throw InternalError("RmRecord: null serialized data");
        }
        int decoded_size = 0;
        memcpy(&decoded_size, data_, sizeof(decoded_size));
        if (decoded_size < 0 || decoded_size > RM_MAX_RECORD_SIZE) {
            throw InvalidRecordSizeError(decoded_size);
        }

        char* new_data = decoded_size == 0 ? nullptr : new char[decoded_size];
        if (decoded_size > 0) {
            memcpy(new_data, data_ + sizeof(int), decoded_size);
        }
        release();
        data = new_data;
        size = decoded_size;
        allocated_ = data != nullptr;
    }

    ~RmRecord() { release(); }

private:
    static char* copy_data(const RmRecord& other) {
        if (other.size < 0 || (other.size > 0 && other.data == nullptr)) {
            throw InternalError("RmRecord: invalid source record");
        }
        if (other.size == 0) {
            return nullptr;
        }
        char* copy = new char[other.size];
        memcpy(copy, other.data, other.size);
        return copy;
    }

    void assign_from(const RmRecord& other) {
        data = copy_data(other);
        size = other.size;
        allocated_ = data != nullptr;
    }

    void release() noexcept {
        if (allocated_) {
            delete[] data;
        }
        allocated_ = false;
        data = nullptr;
        size = 0;
    }
};
