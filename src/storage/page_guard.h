// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <map>

#include "page.h"

class BufferPoolManager;

/**
 * @file page_guard.h
 * @brief 框架内部。Lab1 作业实现 fetch_page / unpin_page，不要实现或改写本文件。
 */

/** @brief 框架内部的 pin 所有者。Lab1 作业实现 fetch/unpin，不要实现本类。 */
class PageGuard {
public:
    PageGuard() noexcept = default;
    ~PageGuard() noexcept;

    PageGuard(const PageGuard&) = delete;
    PageGuard& operator=(const PageGuard&) = delete;

    PageGuard(PageGuard&& other) noexcept;
    PageGuard& operator=(PageGuard&& other) noexcept;

    /** @return guard 当前持有的页面；无效 guard 返回 nullptr。 */
    Page* get() noexcept { return page_; }
    const Page* get() const noexcept { return page_; }
    Page* operator->() noexcept { return page_; }
    const Page* operator->() const noexcept { return page_; }
    Page& operator*() noexcept { return *page_; }
    const Page& operator*() const noexcept { return *page_; }

    /** @return 本次 pin 对应的页面标识；无效 guard 返回默认 PageId。 */
    PageId page_id() const noexcept { return page_id_; }

    /** @return guard 是否持有一次尚未释放的 pin。 */
    explicit operator bool() const noexcept { return page_ != nullptr; }

    /** @brief 记录页面已被修改；释放 pin 时会把脏标记传给缓冲池。 */
    void mark_dirty() noexcept {
        if (page_ != nullptr) {
            dirty_ = true;
        }
    }

    /**
     * @brief 提前释放当前 pin；可重复调用。
     * @return 未持有 pin 或缓冲池成功解固定页面时返回 true。
     */
    [[nodiscard]] bool drop() noexcept;

private:
    friend class BufferPoolManager;

    PageGuard(BufferPoolManager* buffer_pool_manager, Page* page, PageId page_id) noexcept
        : buffer_pool_manager_(buffer_pool_manager),
          page_(page),
          page_id_(page_id) {}

    BufferPoolManager* buffer_pool_manager_ = nullptr;  ///< 非拥有指针，必须比 guard 生命周期更长。
    Page* page_ = nullptr;                              ///< pin 期间保持有效。
    /// 捕获 pin 时的标识，避免依赖可复用帧的后续状态。
    PageId page_id_{};
    bool dirty_ = false;
};

/** @brief 某一时刻所有已固定页面及其 pin_count；仅用于测试前后比较。 */
using PinSnapshot = std::map<PageId, int>;
