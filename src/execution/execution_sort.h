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

/**
 * @brief 按单个字段对子算子的输出排序。
 *
 * 用于 ORDER BY，也作为排序归并连接两侧的输入。输出记录的字段布局与子算子完全相同。
 * 框架只支持单个排序键；如需多键排序，可自行扩展数据结构。
 */
class SortExecutor : public AbstractExecutor {
private:
    std::unique_ptr<AbstractExecutor> prev_;  // 子算子
    ColMeta sort_col_;                        // 排序字段在子算子输出记录中的位置和类型
    bool is_desc_;                            // 是否降序
    // 提示：排序算子需要先读完子算子的全部输出，再按顺序返回。
    // 请自行添加保存记录和迭代位置所需的成员。

public:
    SortExecutor(std::unique_ptr<AbstractExecutor> prev, const TabCol& sel_cols, bool is_desc) {
        prev_ = std::move(prev);
        sort_col_ = *get_col(prev_->cols(), sel_cols);
        is_desc_ = is_desc;
    }

    void begin_tuple() override { throw NotImplementedError("SortExecutor::begin_tuple"); }

    void next_tuple() override { throw NotImplementedError("SortExecutor::next_tuple"); }

    std::unique_ptr<RmRecord> next() override { throw NotImplementedError("SortExecutor::next"); }

    Rid& rid() override { return abstract_rid_; }
};
