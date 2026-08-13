// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cstddef>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "common/config.h"
#include "page.h"
#include "page_guard.h"
#include "replacer/lru_replacer.h"
#include "replacer/replacer.h"

class DiskManager;

class BufferPoolManager {
private:
    size_t pool_size_;                                               ///< 缓冲池帧数。
    std::unique_ptr<Page[]> pages_;                                  ///< 独占的帧数组，下标即 frame_id。
    std::unordered_map<PageId, frame_id_t, PageIdHash> page_table_;  ///< PageId 到帧号。
    std::list<frame_id_t> free_list_;                                ///< 尚未映射磁盘页的空闲帧。
    DiskManager* disk_manager_;                                      ///< 借用，必须比 *this 活得更长。
    std::unique_ptr<Replacer> replacer_;                             ///< 独占的淘汰器，当前为 LRU。
    mutable std::mutex latch_;                                       ///< 保护页表、空闲链表和诊断快照。

public:
    BufferPoolManager(size_t pool_size, DiskManager* disk_manager)
        : pool_size_(pool_size),
          pages_(std::make_unique<Page[]>(pool_size_)),
          disk_manager_(disk_manager),
          replacer_(std::make_unique<LRUReplacer>(pool_size_)) {
        for (size_t i = 0; i < pool_size_; ++i) {
            free_list_.emplace_back(static_cast<frame_id_t>(i));
        }
    }

    ~BufferPoolManager() = default;

    /**
     * @name Lab1 作业接口
     * @brief 实现 pin / unpin。作业不要实现或改写 PageGuard。
     */
    Page* fetch_page(PageId page_id);
    Page* new_page(PageId* page_id);
    bool unpin_page(PageId page_id, bool is_dirty);
    bool flush_page(PageId page_id);
    bool delete_page(PageId page_id);
    void flush_all_pages(int fd);

    /**
     * @brief 将目标页面标记为脏页。
     * @param page 借用，必须指向本缓冲池中的有效帧。
     */
    static void mark_dirty(Page* page) { page->is_dirty_ = true; }

    /** @brief 捕获所有仍被固定页面及其 pin_count，不改变缓冲池状态。 */
    [[nodiscard]] PinSnapshot get_pin_snapshot() const;

    /**
     * @name 框架内部
     * @brief 后续模块使用。Lab1 作业不用写、也不用调用。
     */
    [[nodiscard]] PageGuard fetch_page_guard(PageId page_id);
    [[nodiscard]] PageGuard new_page_guard(PageId* page_id);

private:
    bool find_victim_page(frame_id_t* frame_id);

    void update_page(Page* page, PageId new_page_id, frame_id_t new_frame_id);
};
