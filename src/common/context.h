/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include <string>
#include <vector>

#include "defs.h"
#include "transaction/transaction.h"
#include "transaction/concurrency/lock_manager.h"
#include "recovery/log_manager.h"

// class TransactionManager;

// used for data_send
static int const_offset = -1;

// Structured SELECT result for docs/rmdb_wire.md META/ROW/RESULT_END.
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
    static constexpr size_t kMaxBufferedBytes = 16u * 1024u * 1024u;

    bool has_query_result = false;
    size_t buffered_bytes = 0;
    std::vector<WireResultColumn> columns;
    std::vector<std::vector<WireResultCell>> rows;
};

class Context {
public:
    Context (LockManager *lock_mgr, LogManager *log_mgr, 
            Transaction *txn, char *data_send = nullptr, int *offset = &const_offset)
        : lock_mgr_(lock_mgr), log_mgr_(log_mgr), txn_(txn),
          data_send_(data_send), offset_(offset) {
            ellipsis_ = false;
          }

    // TransactionManager *txn_mgr_;
    LockManager *lock_mgr_;
    LogManager *log_mgr_;
    Transaction *txn_;
    char *data_send_;
    int *offset_;
    bool ellipsis_;
    WireResultSet wire_result_;
};
