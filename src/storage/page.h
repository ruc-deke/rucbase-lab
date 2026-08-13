// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

#include "common/config.h"

/**
 * @file page.h
 * @brief Lab1 认识 `PageId` 和 `Page` 即可。`PageGuard` 在 page_guard.h，属于框架内部。
 */

/**
 * @description: 存储层每个Page的id的声明
 */
struct PageId {
    int fd = -1;  // Page所在的磁盘文件开启后的文件描述符；-1 表示尚未绑定文件。
    page_id_t page_no = INVALID_PAGE_ID;

    friend bool operator==(const PageId& x, const PageId& y) noexcept { return x.fd == y.fd && x.page_no == y.page_no; }
    bool operator<(const PageId& x) const noexcept {
        if (fd != x.fd) return fd < x.fd;
        return page_no < x.page_no;
    }

    std::string to_string() const {
        return "{fd: " + std::to_string(fd) + " page_no: " + std::to_string(page_no) + "}";
    }

    /** @brief 将文件描述符和页号编码为无符号哈希输入，避免有符号移位。 */
    uint64_t hash_key() const noexcept {
        return (static_cast<uint64_t>(static_cast<uint32_t>(fd)) << 16U) | static_cast<uint32_t>(page_no);
    }
};

// PageId的自定义哈希算法, 用于构建unordered_map<PageId, frame_id_t, PageIdHash>
struct PageIdHash {
    size_t operator()(const PageId& x) const noexcept { return std::hash<uint64_t>{}(x.hash_key()); }
};

template <>
struct std::hash<PageId> {
    size_t operator()(const PageId& obj) const noexcept { return std::hash<uint64_t>{}(obj.hash_key()); }
};

/**
 * @description: Page类声明, Page是RMDB数据块的单位、是负责数据操作Record模块的操作对象，
 * Page对象在磁盘上有文件存储, 若在Buffer中则有帧偏移, 并非特指Buffer或Disk上的数据
 */
class Page {
    friend class BufferPoolManager;

public:
    Page() { reset_memory(); }

    ~Page() = default;

    PageId get_page_id() const { return id_; }

    inline char* get_data() { return data_; }

    bool is_dirty() const { return is_dirty_; }

    static constexpr size_t OFFSET_PAGE_START = 0;
    static constexpr size_t OFFSET_LSN = 0;
    static constexpr size_t OFFSET_PAGE_HDR = 4;

    inline lsn_t get_page_lsn() { return *reinterpret_cast<lsn_t*>(get_data() + OFFSET_LSN); }

    inline void set_page_lsn(lsn_t page_lsn) { memcpy(get_data() + OFFSET_LSN, &page_lsn, sizeof(lsn_t)); }

private:
    void reset_memory() { memset(data_, OFFSET_PAGE_START, PAGE_SIZE); }  // 将data_的PAGE_SIZE个字节填充为0

    /** page的唯一标识符 */
    PageId id_;

    /** The actual data that is stored within a page.
     *  该页面在bufferPool中的偏移地址
     */
    alignas(std::max_align_t) char data_[PAGE_SIZE] = {};

    /** 脏页判断 */
    bool is_dirty_ = false;

    /** The pin count of this page. */
    int pin_count_ = 0;
};
