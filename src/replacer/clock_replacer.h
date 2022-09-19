//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// clock_replacer.h
//
// Identification: src/include/buffer/clock_replacer.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <mutex>  // NOLINT
#include <vector>

#include "common/config.h"
#include "replacer/replacer.h"

/**
 * ClockReplacer implements the clock replacement policy, which approximates the Least Recently Used
 * policy.
 */
class ClockReplacer : public Replacer {
    using mutex_t = std::mutex;

   public:
    // EMPTY:     This frame not storage page or pinned (is using by some thread,can not be victim)
    // ACCESSED:  This frame is used by some thread not so long ago
    // UNTOUCHED: This frame can be victim
    enum class Status { UNTOUCHED, ACCESSED, EMPTY_OR_PINNED };
    /**
     * Create a new ClockReplacer.
     * @param num_pages the maximum number of pages the ClockReplacer will be required to store
     */
    explicit ClockReplacer(size_t num_pages);

    /**
     * Destroys the ClockReplacer.
     */
    ~ClockReplacer() override;

    bool Victim(frame_id_t *frame_id) override;

    void Pin(frame_id_t frame_id) override;

    void Unpin(frame_id_t frame_id) override;

    size_t Size() override;

   private:
    std::vector<Status> circular_;
    frame_id_t hand_{0};  // initial hand_ value = 0, the scan starter
    size_t capacity_;
    mutex_t mutex_;
};