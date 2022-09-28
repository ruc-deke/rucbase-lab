#include "log_manager.h"

#include <sstream>

/**
 * 开启日志刷新线程
 */
void LogManager::RunFlushThread() {
    // Todo:
    // 1. 如果系统未开启日志功能，则不能开启日志刷新线程（通过log_mode_判断）
    // 2. 开启一个新线程，用来把flush_buffer_中的内容刷新到磁盘当中
    // 3. 在刷新之前，需要判断当前线程由于哪种原因被唤醒，如果是time_out唤醒，则需要交换log_buffer和flush_buffer
    // 4.  刷新之后需要更新flush_buffer的偏移量、persistent_lsn_等信息

}

/**
 * 辅助函数，用于DiskManager唤醒flush_thread_
 * @param p
 */
void LogManager::WakeUpFlushThread(std::promise<void> *p) {
    {
        std::unique_lock<std::mutex> lock(latch_);
        SwapBuffer();
        SetPromise(p);
    }

    cv_.notify_one();

    // waiting for flush_done
    if (promise != nullptr) {
        promise->get_future().wait();
    }

    SetPromise(nullptr);
}

/**
 * 辅助函数，交换log_buffer_和flush_buffer_及其相关信息
 */
void LogManager::SwapBuffer() {
    std::swap(log_buffer_, flush_buffer_);
    std::swap(log_buffer_write_offset_, flush_buffer_write_offset_);
    flush_lsn_ = next_lsn_ - 1;
}

/**
 * 添加一条日志记录到log_buffer_中
 * @param log_record 要添加的日志记录
 * @return 返回该日志的日志序列号
 */
lsn_t LogManager::AppendLogRecord(LogRecord *log_record) {
    // Todo:
    // 1. 获取互斥锁latch_
    // 2. 判断log_buffer_中是否还存在足够的剩余空间，如果空间不足，需要交换log_buffer_和flush_buffer_，唤醒日志刷新线程
    // 3. 为该日志分配日志序列号
    // 4. 把该日志写入到log_buffer_中

    return log_record->lsn_;
}