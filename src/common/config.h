// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

#define BUFFER_LENGTH 8192

/** 每隔 cycle_detection_interval 指定的时长执行一次环检测。 */
extern std::chrono::milliseconds cycle_detection_interval;

/** 是否启用日志记录。 */
extern std::atomic<bool> enable_logging;

/** 启用日志记录后，按照 log_timeout 指定的间隔将日志刷新到磁盘。 */
extern std::chrono::duration<int64_t> log_timeout;

static constexpr int INVALID_FRAME_ID = -1;     // 无效帧 ID
static constexpr int INVALID_PAGE_ID = -1;      // 无效页面 ID
static constexpr int INVALID_TXN_ID = -1;       // 无效事务 ID
static constexpr int INVALID_TIMESTAMP = -1;    // 无效事务时间戳
static constexpr int INVALID_LSN = -1;          // 无效日志序列号
static constexpr int HEADER_PAGE_ID = 0;        // 文件头页面 ID
static constexpr int PAGE_SIZE = 4096;          // 数据页面大小：4 KB
static constexpr int BUFFER_POOL_SIZE = 65536;  // 缓冲池大小：256 MB
// static constexpr int BUFFER_POOL_SIZE = 262144;  // 缓冲池大小：1 GB
static constexpr int LOG_BUFFER_SIZE = (1024 * PAGE_SIZE);  // 日志缓冲区大小（字节）
static constexpr int BUCKET_SIZE = 50;                      // 可扩展哈希桶容量

using frame_id_t = int32_t;    // 帧 ID 类型；缓冲池以帧为存储单元，一帧对应一页
using page_id_t = int32_t;     // 页面 ID 类型
using txn_id_t = int32_t;      // 事务 ID 类型
using lsn_t = int32_t;         // 日志序列号类型
using slot_offset_t = size_t;  // 槽偏移量类型
using oid_t = uint16_t;
using timestamp_t = int32_t;  // 事务并发控制使用的时间戳类型

inline constexpr char LOG_FILE_NAME[] = "db.log";  ///< 当前数据库的日志文件名。
inline constexpr char DB_META_NAME[] = "db.meta";  ///< 当前数据库的目录文件名。
