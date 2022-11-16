#pragma once
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class ProjectionExecutor : public AbstractExecutor {
   private:
    std::unique_ptr<AbstractExecutor> prev_;
    std::vector<ColMeta> cols_;
    size_t len_;
    std::vector<size_t> sel_idxs_;

   public:
    ProjectionExecutor(std::unique_ptr<AbstractExecutor> prev, const std::vector<TabCol> &sel_cols) {
        prev_ = std::move(prev);

        size_t curr_offset = 0;
        auto &prev_cols = prev_->cols();
        for (auto &sel_col : sel_cols) {
            auto pos = get_col(prev_cols, sel_col);
            sel_idxs_.push_back(pos - prev_cols.begin());
            auto col = *pos;
            col.offset = curr_offset;
            curr_offset += col.len;
            cols_.push_back(col);
        }
        len_ = curr_offset;
    }

    std::string getType() override { return "Projection"; }

    size_t tupleLen() const override { return len_; }

    const std::vector<ColMeta> &cols() const override { return cols_; }

    void beginTuple() override { prev_->beginTuple(); }

    void nextTuple() override {
        assert(!prev_->is_end());
        prev_->nextTuple();
    }

    bool is_end() const override { return prev_->is_end(); }

    std::unique_ptr<RmRecord> Next() override {
        assert(!is_end());
        auto &prev_cols = prev_->cols();
        auto prev_rec = prev_->Next();
        auto &proj_cols = cols_;
        auto proj_rec = std::make_unique<RmRecord>(len_);
        for (size_t proj_idx = 0; proj_idx < proj_cols.size(); proj_idx++) {
            size_t prev_idx = sel_idxs_[proj_idx];
            auto &prev_col = prev_cols[prev_idx];
            auto &proj_col = proj_cols[proj_idx];
            // lab3 task2 Todo
            // 利用memcpy生成proj_rec
            // lab3 task2 Todo End
        }
        return proj_rec;
    }

    void feed(const std::map<TabCol, Value> &feed_dict) override {
        throw InternalError("Cannot feed a projection node");
    }
    Rid &rid() override { return _abstract_rid; }
};