//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// page.h
//
// Identification: src/include/storage/page/page.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//
//                         Rucbase
//
// page.h
//
// Identification: src/storage/page.h
//
// Copyright (c) 2022, RUC Deke Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include "common/config.h"
#include "common/rwlatch.h"

/**
 @brief 存储层每个Page的id的声明
 */
struct PageId {
    int fd;  //  Page所在的磁盘文件开启后的文件描述符, 来定位打开的文件在内存中的位置
    page_id_t page_no = INVALID_PAGE_ID;

    friend bool operator==(const PageId &x, const PageId &y) { return x.fd == y.fd && x.page_no == y.page_no; }
};

// PageId的自定义哈希算法, 用于构建unordered_map<PageId, frame_id_t, PageIdHash>
struct PageIdHash {
    size_t operator()(const PageId &x) const { return (x.fd << 16) | x.page_no; }
};

/**
 @brief Page类声明, Page是rucbase数据块的单位.
 @note Page是负责数据操作Record模块的操作对象.
 @note Page对象在磁盘上有文件存储, 若在Buffer中则有帧偏移, 并非特指Buffer或Disk上的数据
 */
class Page {
    friend class BufferPoolManager;

   public:
    /** Constructor. Zeros out the page data. */
    Page() { ResetMemory(); }

    /** Default destructor. */
    ~Page() = default;

    PageId GetPageId() const { return id_; }

    /** @return the actual data contained within this page */
    inline char *GetData() { return data_; }

    bool IsDirty() const { return is_dirty_; }

    /** Acquire the page write latch. */
    inline void WLatch() { rwlatch_.WLock(); }

    /** Release the page write latch. */
    inline void WUnlatch() { rwlatch_.WUnlock(); }

    /** Acquire the page read latch. */
    inline void RLatch() { rwlatch_.RLock(); }

    /** Release the page read latch. */
    inline void RUnlatch() { rwlatch_.RUnlock(); }

    static constexpr size_t OFFSET_PAGE_START = 0;
    static constexpr size_t OFFSET_LSN = 0;
    static constexpr size_t OFFSET_PAGE_HDR = 4;

    inline lsn_t GetPageLsn() { return *reinterpret_cast<lsn_t *>(GetData() + OFFSET_LSN) ; }

    inline void SetPageLsn(lsn_t page_lsn) { memcpy(GetData() + OFFSET_LSN, &page_lsn, sizeof(lsn_t)); }

   private:
    void ResetMemory() { memset(data_, OFFSET_PAGE_START, PAGE_SIZE); }  // 将data_的PAGE_SIZE个字节填充为0

    /** page的唯一标识符 */
    PageId id_;

    /** The actual data that is stored within a page.
     *  该页面在bufferPool中的偏移地址
     */
    char data_[PAGE_SIZE] = {};

    /** 脏页判断 */
    bool is_dirty_ = false;

    /** The pin count of this page. */
    int pin_count_ = 0;

    /** Page latch. */
    ReaderWriterLatch rwlatch_;
};