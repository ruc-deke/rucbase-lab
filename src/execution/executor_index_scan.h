#pragma once

#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class IndexScanExecutor : public AbstractExecutor {
   private:
    std::string tab_name_;
    std::vector<Condition> conds_;
    RmFileHandle *fh_;
    std::vector<ColMeta> cols_;
    size_t len_;
    std::vector<Condition> fed_conds_;

    int index_no_;

    Rid rid_;
    std::unique_ptr<RecScan> scan_;

    SmManager *sm_manager_;

   public:
    IndexScanExecutor(SmManager *sm_manager, std::string tab_name, std::vector<Condition> conds, int index_no,
                      Context *context) {
        // lab3 task2 todo
        // 参考seqscan作法,实现indexscan构造方法
        // lab3 task2 todo
    }

    std::string getType() { return "indexScan"; }

    void beginTuple() {
        check_runtime_conds();

        // index is available, scan index
        auto ih = sm_manager_->ihs_.at(sm_manager_->get_ix_manager()->get_index_name(tab_name_, index_no_)).get();
        Iid lower = ih->leaf_begin();
        Iid upper = ih->leaf_end();
        auto &index_col = cols_[index_no_];
        for (auto &cond : fed_conds_) {
            if (cond.is_rhs_val && cond.op != OP_NE && cond.lhs_col.col_name == index_col.name) {
                char *rhs_key = cond.rhs_val.raw->data;
                if (cond.op == OP_EQ) {
                    lower = ih->lower_bound(rhs_key);
                    upper = ih->upper_bound(rhs_key);
                }

                // lab3 task2 todo
                /**
                 * else if(cond.op == ?){
                 *
                 * }else if(){
                 * ...
                 * }else if(){
                 * ...
                 * }
                 * ...
                 * else{
                 *  throw InternalError("Unexpected op type");
                 * }
                 *
                 *
                 */
                // 利用cond 进行索引扫描
                // lab3 task2 todo end
                break;
            }
        }
        scan_ = std::make_unique<IxScan>(ih, lower, upper, sm_manager_->get_bpm());
        // Get the first record
        while (!scan_->is_end()) {
            rid_ = scan_->rid();
            auto rec = fh_->get_record(rid_, context_);
            if (eval_conds(cols_, fed_conds_, rec.get())) {
                break;
            }
            scan_->next();
        }
    }

    void nextTuple() {
        check_runtime_conds();
        assert(!is_end());
        // lab3 task2 todo
        // 扫描到下一个满足条件的记录,赋rid_,中止循环
        // lab3 task2 todo end
    }

    bool is_end() const override { return scan_->is_end(); }

    size_t tupleLen() const override { return len_; }

    const std::vector<ColMeta> &cols() const override { return cols_; }

    std::unique_ptr<RmRecord> Next() override {
        assert(!is_end());
        return fh_->get_record(rid_, context_);
    }

    void feed(const std::map<TabCol, Value> &feed_dict) override {
        fed_conds_ = conds_;
        for (auto &cond : fed_conds_) {
            // lab3 task2 todo
            // 参考seqscan
            // lab3 task2 todo end
        }
        check_runtime_conds();
    }

    Rid &rid() override { return rid_; }

    void check_runtime_conds() {
        for (auto &cond : fed_conds_) {
            assert(cond.lhs_col.tab_name == tab_name_);
            if (!cond.is_rhs_val) {
                assert(cond.rhs_col.tab_name == tab_name_);
            }
        }
    }

    bool eval_cond(const std::vector<ColMeta> &rec_cols, const Condition &cond, const RmRecord *rec) {
        auto lhs_col = get_col(rec_cols, cond.lhs_col);
        char *lhs = rec->data + lhs_col->offset;
        char *rhs;
        ColType rhs_type;
        if (cond.is_rhs_val) {
            rhs_type = cond.rhs_val.type;
            rhs = cond.rhs_val.raw->data;
        } else {
            // rhs is a column
            auto rhs_col = get_col(rec_cols, cond.rhs_col);
            rhs_type = rhs_col->type;
            rhs = rec->data + rhs_col->offset;
        }
        assert(rhs_type == lhs_col->type);  // TODO convert to common type
        int cmp = ix_compare(lhs, rhs, rhs_type, lhs_col->len);
        if (cond.op == OP_EQ) {
            return cmp == 0;
        } else if (cond.op == OP_NE) {
            return cmp != 0;
        } else if (cond.op == OP_LT) {
            return cmp < 0;
        } else if (cond.op == OP_GT) {
            return cmp > 0;
        } else if (cond.op == OP_LE) {
            return cmp <= 0;
        } else if (cond.op == OP_GE) {
            return cmp >= 0;
        } else {
            throw InternalError("Unexpected op type");
        }
    }

    bool eval_conds(const std::vector<ColMeta> &rec_cols, const std::vector<Condition> &conds, const RmRecord *rec) {
        return std::all_of(conds.begin(), conds.end(),
                           [&](const Condition &cond) { return eval_cond(rec_cols, cond, rec); });
    }
};