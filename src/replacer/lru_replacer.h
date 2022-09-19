#pragma once

#include <list>
#include <mutex>  // NOLINT 包含std::mutex、std::scoped_lock
#include <vector>

#include "common/config.h"
#include "replacer/replacer.h"
#include "unordered_map"

/* PROJECT #1 - BUFFER POOL | TASK #1 - LRU REPLACEMENT POLICY
LRUReplacer的大小与缓冲池相同，因为它包含BufferPoolManager中所有帧的占位符。
LRUReplacer被初始化为没有frame。在LRUReplacer中只会考虑新取消固定的frame。
实现课程中讨论的LRU策略，实现以下方法：
1. Victim(T*): Replacer跟踪与所有元素相比最近最少访问的对象并删除，将其页号存储在输出参数中并返回True，为空则返回False。
2. Pin(T)：在将page固定到BufferPoolManager中的frame后，应调用此方法。它应该从LRUReplacer中删除包含固定page的frame。
3. Unpin(T)：当page的pin_count变为0时应调用此方法。此方法应将包含未固定page的frame添加到LRUReplacer。
4. Size()：此方法返回当前在LRUReplacer中的frame数量。
实现细节由您决定。您可以使用内置的STL容器。您可以假设不会耗尽内存，但必须确保操作是线程安全的。
*/

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
