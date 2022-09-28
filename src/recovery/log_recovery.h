#pragma once

#include "log_record.h"
#include "system/sm_manager.h"

class LogRecovery {
   public:
    LogRecovery(SmManager *sm_manager, DiskManager *disk_manager) {
        log_buffer_ = new char[LOG_BUFFER_SIZE];
        log_offset_ = 0;
        active_txns_ = std::unordered_map<txn_id_t, lsn_t>();
        lsn_mapping_ = std::unordered_map<lsn_t, int>();
        sm_manager_ = sm_manager;
        disk_manager_ = disk_manager;
    }

    ~LogRecovery() {
        delete[] log_buffer_;
        sm_manager_ = nullptr;
        disk_manager_ = nullptr;
    }

    void Redo();
    void Undo();
    inline bool GetRecoveryMode() { return recovery_mode_; }

    bool DeserializeLogRecord(const char* data, LogRecord &log_record);

   private:
    // store the running transactions, the mapping of running transactions to their lastest log records
    std::unordered_map<txn_id_t, lsn_t> active_txns_;   // 活动事务列表，记录当前系统运行过程中所有正在执行的事务
    std::unordered_map<lsn_t, int> lsn_mapping_;
    char *log_buffer_;      // 从磁盘中读取的日志记录
    int log_offset_;        // log_buffer_的偏移量
    SmManager *sm_manager_;
    DiskManager *disk_manager_;
    bool recovery_mode_ = false; // 用于标识在系统开启时是否进行系统故障恢复
};