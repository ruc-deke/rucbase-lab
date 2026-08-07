// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "ix_scan.h"

/**
 * @brief 
 * @todo 加上读锁（需要使用缓冲池得到page）
 */
void IxScan::next() {
    assert(!is_end());
    IxNodeHandle *node = ih_->fetch_node(iid_.page_no);
    assert(node->is_leaf_page());
    assert(iid_.slot_no < node->get_size());
    // increment slot no
    iid_.slot_no++;
    if (iid_.page_no != ih_->file_hdr_->last_leaf_ && iid_.slot_no == node->get_size()) {
        // go to next leaf
        iid_.slot_no = 0;
        iid_.page_no = node->get_next_leaf();
    }
}

Rid IxScan::rid() const {
    return ih_->get_rid(iid_);
}