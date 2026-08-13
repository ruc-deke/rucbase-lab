// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <vector>

#include "common/config.h"
#include "log_manager.h"

class BufferPoolManager;
class DiskManager;
class RmFileHandle;
class SmManager;

class RedoLogsInPage {
public:
    RedoLogsInPage() { table_file_ = nullptr; }
    RmFileHandle* table_file_;
    std::vector<lsn_t> redo_logs_;  // 在该page上需要redo的操作的lsn
};

class RecoveryManager {
public:
    RecoveryManager(DiskManager* disk_manager, BufferPoolManager* buffer_pool_manager, SmManager* sm_manager) {
        disk_manager_ = disk_manager;
        buffer_pool_manager_ = buffer_pool_manager;
        sm_manager_ = sm_manager;
    }

    void analyze();
    void redo();
    void undo();

private:
    LogBuffer buffer_;                        // 读入日志
    DiskManager* disk_manager_;               // 用来读写文件
    BufferPoolManager* buffer_pool_manager_;  // 对页面进行读写
    SmManager* sm_manager_;                   // 访问数据库元数据
};
