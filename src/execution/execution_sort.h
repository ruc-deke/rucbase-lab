// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "common/common.h"
#include "common/defs.h"
#include "common/errors.h"
#include "executor_abstract.h"
#include "record/rm_defs.h"
#include "system/sm_meta.h"

class SortExecutor : public AbstractExecutor {
private:
    std::unique_ptr<AbstractExecutor> prev_;
    ColMeta cols_;  // 框架中只支持一个键排序，需要自行修改数据结构支持多个键排序
    size_t tuple_num;
    bool is_desc_;
    std::vector<size_t> used_tuple;
    std::unique_ptr<RmRecord> current_tuple;

public:
    SortExecutor(std::unique_ptr<AbstractExecutor> prev, const TabCol& sel_cols, bool is_desc) {
        prev_ = std::move(prev);
        cols_ = prev_->get_col_offset(sel_cols);
        is_desc_ = is_desc;
        tuple_num = 0;
        used_tuple.clear();
    }

    void begin_tuple() override { throw NotImplementedError("SortExecutor::begin_tuple"); }

    void next_tuple() override { throw NotImplementedError("SortExecutor::next_tuple"); }

    std::unique_ptr<RmRecord> next() override { throw NotImplementedError("SortExecutor::next"); }

    Rid& rid() override { return abstract_rid_; }
};
