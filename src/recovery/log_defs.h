// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include "defs.h"
#include "storage/disk_manager.h"
#include "common/config.h"

#include <atomic>
#include <chrono>

static constexpr std::chrono::duration<int64_t> FLUSH_TIMEOUT = std::chrono::seconds(3);
// the offset of log_type_ in log header
static constexpr int OFFSET_LOG_TYPE = 0;
// the offset of lsn_ in log header
static constexpr int OFFSET_LSN = sizeof(int);
// the offset of log_tot_len_ in log header
static constexpr int OFFSET_LOG_TOT_LEN = OFFSET_LSN + sizeof(lsn_t);
// the offset of log_tid_ in log header
static constexpr int OFFSET_LOG_TID = OFFSET_LOG_TOT_LEN + sizeof(uint32_t);
// the offset of prev_lsn_ in log header
static constexpr int OFFSET_PREV_LSN = OFFSET_LOG_TID + sizeof(txn_id_t);
// offset of log data
static constexpr int OFFSET_LOG_DATA = OFFSET_PREV_LSN + sizeof(lsn_t);
// sizeof log_header
static constexpr int LOG_HEADER_SIZE = OFFSET_LOG_DATA;

