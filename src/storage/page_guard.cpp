// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "page_guard.h"

#include <utility>

#include "buffer_pool_manager.h"

PageGuard::~PageGuard() noexcept { (void)drop(); }

PageGuard::PageGuard(PageGuard&& other) noexcept
    : buffer_pool_manager_(std::exchange(other.buffer_pool_manager_, nullptr)),
      page_(std::exchange(other.page_, nullptr)),
      page_id_(std::exchange(other.page_id_, PageId{})),
      dirty_(std::exchange(other.dirty_, false)) {}

PageGuard& PageGuard::operator=(PageGuard&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    (void)drop();
    buffer_pool_manager_ = std::exchange(other.buffer_pool_manager_, nullptr);
    page_ = std::exchange(other.page_, nullptr);
    page_id_ = std::exchange(other.page_id_, PageId{});
    dirty_ = std::exchange(other.dirty_, false);
    return *this;
}

bool PageGuard::drop() noexcept {
    if (page_ == nullptr) {
        return true;
    }

    BufferPoolManager* buffer_pool_manager = std::exchange(buffer_pool_manager_, nullptr);
    page_ = nullptr;
    PageId page_id = std::exchange(page_id_, PageId{});
    bool dirty = std::exchange(dirty_, false);
    if (buffer_pool_manager == nullptr) {
        return false;
    }

    return buffer_pool_manager->unpin_page(page_id, dirty);
}

PageGuard BufferPoolManager::fetch_page_guard(PageId page_id) {
    Page* page = fetch_page(page_id);
    if (page == nullptr) {
        return {};
    }
    return PageGuard(this, page, page->get_page_id());
}

PageGuard BufferPoolManager::new_page_guard(PageId* page_id) {
    Page* page = new_page(page_id);
    if (page == nullptr) {
        return {};
    }
    return PageGuard(this, page, page->get_page_id());
}

PinSnapshot BufferPoolManager::get_pin_snapshot() const {
    PinSnapshot snapshot;
    std::scoped_lock lock{latch_};
    for (size_t index = 0; index < pool_size_; ++index) {
        const Page& page = pages_[index];
        if (page.pin_count_ > 0) {
            snapshot[page.id_] += page.pin_count_;
        }
    }
    return snapshot;
}
