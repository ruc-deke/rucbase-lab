// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "index_scan.h"

#include <cassert>

#include "b_plus_tree.h"

/** @brief 前进到下一个索引项，必要时跨越到后继叶子页。 */
void IndexScan::next() {
    assert(!is_end());
    BPlusTreeNode* node = tree_->fetch_node(position_.page_no);
    assert(node->is_leaf_page());
    assert(position_.slot_no < node->get_size());
    ++position_.slot_no;
    if (position_.page_no != tree_->file_header_->last_leaf_ && position_.slot_no == node->get_size()) {
        position_.slot_no = 0;
        position_.page_no = node->get_next_leaf();
    }
    tree_->unpin_node(node, false);
}

Rid IndexScan::rid() const { return tree_->get_rid(position_); }
