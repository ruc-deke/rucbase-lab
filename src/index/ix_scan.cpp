#include "ix_scan.h"

/**
 * @brief 找到leaf page的下一个slot_no
 */
void IxScan::next() {
    assert(!is_end());
    IxNodeHandle *node = ih_->FetchNode(iid_.page_no);
    assert(node->IsLeafPage());
    assert(iid_.slot_no < node->GetSize());
    // increment slot no
    iid_.slot_no++;
    if (iid_.page_no != ih_->file_hdr_.last_leaf && iid_.slot_no == node->GetSize()) {
        // go to next leaf
        iid_.slot_no = 0;
        iid_.page_no = node->GetNextLeaf();
    }
}

Rid IxScan::rid() const {
    return ih_->get_rid(iid_);
}
