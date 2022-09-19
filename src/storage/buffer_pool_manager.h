//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// buffer_pool_manager.h
//
// Identification: src/include/buffer/buffer_pool_manager.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
//
//                         Rucbase
//
// buffer_pool_manager.h
//
// Identification: src/storage/buffer_pool_manager.h
//
// Copyright (c) 2022, RUC Deke Group
//
//===----------------------------------------------------------------------===//

#pragma once
#include <fcntl.h>
#include <unistd.h>

#include <cassert>
#include <list>
#include <unordered_map>
#include <vector>

#include "common/logger.h"  // for debug
#include "disk_manager.h"
#include "errors.h"
#include "page.h"
#include "replacer/clock_replacer.h"
#include "replacer/lru_replacer.h"
#include "replacer/replacer.h"

class BufferPoolManager {
   private:
    /**
     * @brief Number of pages in the buffer pool.
     */
    size_t pool_size_;
    /**
     * @brief BufferPool中的Page对象数组(指针)
     * @note 在构造函数中申请内存空间,折构函数中释放,大小为BUFFER_POOL_SIZE
     */
    Page *pages_;
    /**
     * @brief 以自定义PageIdHash为哈希函数的<PageId,frame_id_t>哈希表.
     * @note 用于根据PageId定位其在BufferPool中的frame_id_t
     */
    std::unordered_map<PageId, frame_id_t, PageIdHash> page_table_;
    /**
     * @brief BufferPool空闲帧的id构成的链表
     */
    std::list<frame_id_t> free_list_;
    /** 上层传入disk_manager */
    DiskManager *disk_manager_;

    /**
     * @brief BufferPool页面替换策略类
     *
     */
    Replacer *replacer_;

    /** This latch protects shared data structures */
    std::mutex latch_;

   public:
    BufferPoolManager(size_t pool_size, DiskManager *disk_manager)
        : pool_size_(pool_size), disk_manager_(disk_manager) {
        // We allocate a consecutive memory space for the buffer pool.
        pages_ = new Page[pool_size_];
        // can be changed to ClockReplacer
        if (REPLACER_TYPE.compare("LRU"))
            replacer_ = new LRUReplacer(pool_size_);
        else if (REPLACER_TYPE.compare("CLOCK"))
            replacer_ = new LRUReplacer(pool_size_);
        else {
            LOG_WARN("BufferPoolManager Replacer type defined wrong, use LRU as replacer.\n");
            replacer_ = new LRUReplacer(pool_size_);
        }
        // Initially, every page is in the free list.
        for (size_t i = 0; i < pool_size_; ++i) {
            free_list_.emplace_back(static_cast<frame_id_t>(i));  // static_cast转换数据类型
        }
    }

    /**
     * @brief Destroy the Buffer Pool object
     *
     */
    ~BufferPoolManager() {
        delete[] pages_;
        delete replacer_;
    }

   public:
    /**
     * Fetch the requested page from the buffer pool.
     * @param page_id id of page to be fetched
     * @return the requested page
     */
    Page *FetchPage(PageId page_id);

    /**
     * Unpin the target page from the buffer pool.
     * @param page_id id of page to be unpinned
     * @param is_dirty true if the page should be marked as dirty, false otherwise
     * @return false if the page pin count is <= 0 before this call, true otherwise
     */
    bool UnpinPage(PageId page_id, bool is_dirty);

    /**
     * Flushes the target page to disk.
     * @param page_id id of page to be flushed, cannot be INVALID_PAGE_ID
     * @return false if the page could not be found in the page table, true otherwise
     */
    bool FlushPage(PageId page_id);

    /**
     * Creates a new page in the buffer pool.
     * @param[out] page_id id of created page
     * @return nullptr if no new pages could be created, otherwise pointer to new page
     */
    Page *NewPage(PageId *page_id);

    /**
     * Deletes a page from the buffer pool.
     * @param page_id id of page to be deleted
     * @return false if the page exists but could not be deleted, true if the page didn't exist or deletion succeeded
     */
    bool DeletePage(PageId page_id);

    /**
     * Flushes all the pages in the buffer pool to disk.
     */
    void FlushAllPages(int fd);

   private:
    bool FindVictimPage(frame_id_t *frame_id);

    void UpdatePage(Page *page, PageId new_page_id, frame_id_t new_frame_id);
};