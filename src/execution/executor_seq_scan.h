#pragma once

#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class SeqScanExecutor : public AbstractExecutor {
   private:
    std::string tab_name_;
    std::vector<Condition> conds_;  // 初始扫描条件(来自SQL)
    RmFileHandle *fh_;              // TableHeap
    std::vector<ColMeta> cols_;
    size_t len_;
    std::vector<Condition> fed_conds_;  // 实际扫描条件(可能由于连接运算动态改变)

    Rid rid_;                        // 当前扫描到的记录的rid
    std::unique_ptr<RecScan> scan_;  // table_iterator

    SmManager *sm_manager_;

   public:
    SeqScanExecutor(SmManager *sm_manager, std::string tab_name, std::vector<Condition> conds, Context *context) {
        sm_manager_ = sm_manager;
        tab_name_ = std::move(tab_name);
        conds_ = std::move(conds);
        TabMeta &tab = sm_manager_->db_.get_table(tab_name_);
        fh_ = sm_manager_->fhs_.at(tab_name_).get();
        cols_ = tab.cols;
        len_ = cols_.back().offset + cols_.back().len;
        context_ = context;
        std::map<CompOp, CompOp> swap_op = {
            {OP_EQ, OP_EQ}, {OP_NE, OP_NE}, {OP_LT, OP_GT}, {OP_GT, OP_LT}, {OP_LE, OP_GE}, {OP_GE, OP_LE},
        };

        for (auto &cond : conds_) {
            if (cond.lhs_col.tab_name != tab_name_) {
                // lhs is on other table, now rhs must be on this table
                assert(!cond.is_rhs_val && cond.rhs_col.tab_name == tab_name_);
                // swap lhs and rhs
                std::swap(cond.lhs_col, cond.rhs_col);
                cond.op = swap_op.at(cond.op);
            }
        }
        fed_conds_ = conds_;
    }

    std::string getType() override { return "SeqScan"; }

    /**
     * @brief 构建表迭代器scan_,并开始迭代扫描,直到扫描到第一个满足谓词条件的元组停止,并赋值给rid_
     *
     */
    void beginTuple() override {
        check_runtime_conds();

        scan_ = std::make_unique<RmScan>(fh_);

        // 得到第一个满足fed_conds_条件的record,并把其rid赋给算子成员rid_
        while (!scan_->is_end()) {
            rid_ = scan_->rid();
            try {
                auto rec = fh_->get_record(rid_, context_);  // TableHeap->GetTuple() 当前扫描到的记录
                // lab3 task2 todo
                // 利用eval_conds判断是否当前记录(rec.get())满足谓词条件
                // 满足则中止循环
                // lab3 task2 todo end
            } catch (RecordNotFoundError &e) {
                std::cerr << e.what() << std::endl;
            }

            scan_->next();  // 找下一个有record的位置
        }
    }

    void nextTuple() override {
        check_runtime_conds();
        assert(!is_end());
        for (scan_->next(); !scan_->is_end(); scan_->next()) {  // 用TableIterator遍历TableHeap中的所有Tuple
            // lab3 task2 todo
            // 获取当前记录(参考beginTuple())赋给算子成员rid_
            // 利用eval_conds判断是否当前记录(rec.get())满足谓词条件
            // 满足则中止循环
            // lab3 task2 todo End
        }
    }

    bool is_end() const override { return scan_->is_end(); }

    size_t tupleLen() const override { return len_; }

    const std::vector<ColMeta> &cols() const override { return cols_; }

    std::unique_ptr<RmRecord> Next() override {
        // lab3 task2 todo
        // 利用fh_得到记录record
        // lab3 task2 todo end
    }

    void feed(const std::map<TabCol, Value> &feed_dict) override {
        fed_conds_ = conds_;
        for (auto &cond : fed_conds_) {
            // eg. where B.id=A.id , this table is B
            // cond.rhs is not a val but a column
            // and the rhs column's tab name is not this tab
            // need to feed rhs col
            // set the A.id to a val,  the val is  current Table_A Tuple's map.getValue('id');
            if (!cond.is_rhs_val && cond.rhs_col.tab_name != tab_name_) {
                cond.is_rhs_val = true;
                cond.rhs_val = feed_dict.at(cond.rhs_col);
            }
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