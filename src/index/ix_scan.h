#pragma once

#include "ix_defs.h"
#include "ix_index_handle.h"

/**
 * @brief 用于遍历叶子结点
 */
class IxScan : public RecScan {
    const IxIndexHandle *ih_;
    Iid iid_;  // 初始为lower（用于遍历的指针）
    Iid end_;  // 初始为upper
    BufferPoolManager *bpm_;

   public:
    IxScan(const IxIndexHandle *ih, const Iid &lower, const Iid &upper, BufferPoolManager *bpm);

    void next() override;

    bool is_end() const override;

    Rid rid() const override;

    const Iid &iid() const;
};
