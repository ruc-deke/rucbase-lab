// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cstddef>
#include <iterator>
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
 * @brief 排序归并连接（Sort-Merge Join）。
 *
 * 左右孩子的输出都已按各自的连接键升序排列（Planner 会在两侧放置 SortExecutor）。
 * 算子同时推进左右两侧，只输出连接键相等、且满足其余连接条件的记录对。
 * 输出记录的布局与嵌套循环连接相同：左孩子记录在前，右孩子记录在后。
 *
 * @note 两侧都可能出现连接键重复的记录。火山模型的孩子算子只能向前迭代，不能回退。
 */
class MergeJoinExecutor : public AbstractExecutor {
private:
    std::unique_ptr<AbstractExecutor> left_;   // 左孩子，按 left_key_ 升序输出
    std::unique_ptr<AbstractExecutor> right_;  // 右孩子，按 right_key_ 升序输出
    size_t len_;                               // 连接后每条记录的长度
    std::vector<ColMeta> cols_;                // 连接后记录的字段；右孩子字段的 offset 已加上左记录长度

    Condition merge_cond_;                   // 归并键：left_key_ = right_key_
    ColMeta left_key_;                       // 连接键在左孩子记录中的位置
    ColMeta right_key_;                      // 连接键在右孩子记录中的位置
    std::vector<Condition> residual_conds_;  // 其余连接条件，在连接键相等后继续检查
    // 请自行添加归并过程所需的状态。

public:
    /**
     * @param conds conds[0] 为归并键（列与列的 OP_EQ，lhs 来自左孩子、rhs 来自右孩子），其余为附加条件。
     * @throws InternalError conds 为空或 conds[0] 不是列与列的等值条件。
     */
    MergeJoinExecutor(std::unique_ptr<AbstractExecutor> left,
                      std::unique_ptr<AbstractExecutor> right,
                      std::vector<Condition> conds) {
        if (conds.empty() || conds.front().is_rhs_val || conds.front().op != OP_EQ) {
            throw InternalError("MergeJoinExecutor requires a column equality as its first condition");
        }
        left_ = std::move(left);
        right_ = std::move(right);
        len_ = left_->tuple_len() + right_->tuple_len();
        cols_ = left_->cols();
        auto right_cols = right_->cols();
        for (auto& col : right_cols) {
            col.offset += static_cast<int>(left_->tuple_len());
        }
        cols_.insert(cols_.end(), right_cols.begin(), right_cols.end());

        merge_cond_ = conds.front();
        left_key_ = *get_col(left_->cols(), merge_cond_.lhs_col);
        right_key_ = *get_col(right_->cols(), merge_cond_.rhs_col);
        residual_conds_.assign(std::make_move_iterator(conds.begin() + 1), std::make_move_iterator(conds.end()));
    }

    void begin_tuple() override { throw NotImplementedError("MergeJoinExecutor::begin_tuple"); }

    void next_tuple() override { throw NotImplementedError("MergeJoinExecutor::next_tuple"); }

    std::unique_ptr<RmRecord> next() override { throw NotImplementedError("MergeJoinExecutor::next"); }

    Rid& rid() override { return abstract_rid_; }
};
