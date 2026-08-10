// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <string>
#include <vector>

#include "common/defs.h"
#include "recovery/log_manager.h"
#include "transaction/concurrency/lock_manager.h"
#include "transaction/transaction.h"

// class TransactionManager;

// used for data_send
static int const_offset = -1;

// Structured tabular result for META/ROW/RESULT_END wire responses.
struct WireResultColumn {
    std::string name;
    ColType type = TYPE_STRING;
};

struct WireResultCell {
    ColType type = TYPE_STRING;
    int int_val = 0;
    float float_val = 0.0f;
    std::string str_val;
};

struct WireResultSet {
    // Conservative budget for retained result objects and string contents. It
    // bounds teaching-server memory even when a result has many tiny cells.
    static constexpr size_t kMaxBufferedBytes = size_t{16} * 1024 * 1024;

    bool has_query_result = false;
    size_t buffered_bytes = 0;
    std::vector<WireResultColumn> columns;
    std::vector<std::vector<WireResultCell>> rows;

    bool try_account(size_t bytes) noexcept {
        if (buffered_bytes > kMaxBufferedBytes || bytes > kMaxBufferedBytes - buffered_bytes) {
            return false;
        }
        buffered_bytes += bytes;
        return true;
    }
};

class Context {
public:
    Context(LockManager* lock_mgr,
            LogManager* log_mgr,
            Transaction* txn,
            char* data_send = nullptr,
            int* offset = &const_offset)
        : lock_mgr_(lock_mgr),
          log_mgr_(log_mgr),
          txn_(txn),
          data_send_(data_send),
          offset_(offset) {}

    // TransactionManager *txn_mgr_;
    LockManager* lock_mgr_;
    LogManager* log_mgr_;
    Transaction* txn_;
    char* data_send_;
    int* offset_;
    WireResultSet wire_result_;
};
