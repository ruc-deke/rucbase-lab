#pragma once

#include "log_record.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>

/**
 * TODO: PROBLEM{ if the log_record.size is large than the page_size }
 */

class LogManager{
public:

    explicit LogManager(DiskManager *disk_manager) {
        next_lsn_ = 0;
        flush_lsn_ = INVALID_LSN;
        persistent_lsn_ = INVALID_LSN;
        log_buffer_ = new char[LOG_BUFFER_SIZE];
        std::memset(log_buffer_, 0, LOG_BUFFER_SIZE);
        flush_buffer_ = new char[LOG_BUFFER_SIZE];
        std::memset(flush_buffer_, 0, LOG_BUFFER_SIZE);
        disk_manager_ = disk_manager;
    }

    ~LogManager() {
        delete[] log_buffer_;
        delete[] flush_buffer_;
        log_buffer_ = nullptr;
        flush_buffer_ = nullptr;
        disk_manager_ = nullptr;
    }

    /**
     * @brief create the flush_thread to flush the log records into disk
     */
    void RunFlushThread();
    void StopFlushThread() { log_mode_ = false; flush_thread_->join(); delete flush_thread_; }
    /**
     * @brief swap the log_buffer and flush_buffer in order to flush records into disk
     */
    void SwapBuffer();
    /**
     * @brief provided for bufferpool
     * when bufferpool wants to force the flush, it can call this function
     */
    void WakeUpFlushThread(std::promise<void> *p);

    lsn_t AppendLogRecord(LogRecord * log_record);

    inline bool GetLogMode() { return log_mode_; }
    inline void SetLogMode(bool log_mode) { log_mode_ = log_mode; }
    inline lsn_t GetNextLsn() { return next_lsn_; }
    inline lsn_t GetFlushLsn() { return flush_lsn_; }
    inline char * GetLogBuffer() { return log_buffer_; }
    inline lsn_t GetPersistentLsn() { return persistent_lsn_; }
    inline void SetPromise(std::promise<void> *p) { promise = p; }

private:
    bool log_mode_{false};   // 标识系统是否开启日志功能，默认开启日志功能，如果不开启日志功能，需要设置该变量为false

    char *log_buffer_; // 用来暂时存储系统运行过程中添加的日志; append log_record into log_buffer
    char *flush_buffer_; // 用来暂时存储需要刷新到磁盘中的日志; flush the logs in flush_buffer into disk file

    std::atomic<lsn_t> next_lsn_; // 用于分发日志序列号; next lsn in the system
    std::atomic<lsn_t> persistent_lsn_; // 已经刷新到磁盘中的最后一条日志的日志序列号; the last persistent lsn
    lsn_t flush_lsn_; // flush_buffer_中最后一条日志的日志记录号; the last lsn in the flush_buffer

    size_t log_buffer_write_offset_ = 0; // log_buffer_的偏移量
    size_t flush_buffer_write_offset_ = 0; // flush_buffer_的偏移量

    std::thread *flush_thread_; // 日志刷新线程

    std::mutex latch_; // 互斥锁，用于log_buffer_的互斥访问

    std::condition_variable cv_; // 条件变量，用于flush_thread的唤醒; to notify the flush_thread

    // used to notify the WakeUpFlushThread() that the log records have been flushed into the disk
    std::promise<void> *promise; // 主动唤醒flush_thread_，强制要求刷新日志

    DiskManager *disk_manager_;
};