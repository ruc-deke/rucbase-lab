//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_replacer.h
//
// Identification: src/include/buffer/lru_replacer.h
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once

#include <list>
#include <mutex>  // NOLINT 包含std::mutex、std::scoped_lock
#include <vector>

#include "common/config.h"
#include "replacer/replacer.h"
#include "unordered_map"

/**
 * LRUReplacer implements the lru replacement policy, which approximates the Least Recently Used policy.
 */
class LRUReplacer : public Replacer {
   public:
    /**
     * Create a new LRUReplacer.
     * @param num_pages the maximum number of pages the LRUReplacer will be required to store
     */
    explicit LRUReplacer(size_t num_pages);
    // explicit关键字只能用来修饰类内部的构造函数声明，作用于单个参数的构造函数；被修饰的构造函数的类，不能发生相应的隐式类型转换。

    /**
     * Destroys the LRUReplacer.
     */
    ~LRUReplacer();

    bool Victim(frame_id_t *frame_id);

    void Pin(frame_id_t frame_id);

    void Unpin(frame_id_t frame_id);

    size_t Size();

   private:
    std::mutex latch_;               // 互斥锁
    std::list<frame_id_t> LRUlist_;  // 按加入的时间顺序存放unpinned pages的frame id，首部表示最近被访问
    std::unordered_map<frame_id_t, std::list<frame_id_t>::iterator> LRUhash_;  // frame_id_t -> unpinned pages的frame id
    size_t max_size_;  // 最大容量（与缓冲池的容量相同）
};
