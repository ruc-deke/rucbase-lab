// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include "common/defs.h"
#include "index_types.h"

class BPlusTree;

/** @brief 沿 B+ 树叶子链表顺序遍历记录位置。 */
class IndexScan : public RecScan {
private:
    const BPlusTree* tree_;
    IndexPosition position_;
    IndexPosition end_;

public:
    IndexScan(const BPlusTree* tree, const IndexPosition& begin, const IndexPosition& end)
        : tree_(tree),
          position_(begin),
          end_(end) {}

    void next() override;

    bool is_end() const override { return position_ == end_; }

    Rid rid() const override;

    const IndexPosition& position() const noexcept { return position_; }
};
