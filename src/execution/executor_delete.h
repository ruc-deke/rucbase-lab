#pragma once
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

class DeleteExecutor : public AbstractExecutor {
   private:
    TabMeta tab_;
    std::vector<Condition> conds_;
    RmFileHandle *fh_;
    std::vector<Rid> rids_;
    std::string tab_name_;
    SmManager *sm_manager_;

   public:
    DeleteExecutor(SmManager *sm_manager, const std::string &tab_name, std::vector<Condition> conds,
                   std::vector<Rid> rids, Context *context) {
        sm_manager_ = sm_manager;
        tab_name_ = tab_name;
        tab_ = sm_manager_->db_.get_table(tab_name);
        fh_ = sm_manager_->fhs_.at(tab_name).get();
        conds_ = conds;
        rids_ = rids;
        context_ = context;
    }
    std::unique_ptr<RmRecord> Next() override {
        // Get all index files
        std::vector<IxIndexHandle *> ihs(tab_.cols.size(), nullptr);
        for (size_t col_i = 0; col_i < tab_.cols.size(); col_i++) {
            if (tab_.cols[col_i].index) {
                // lab3 task3 Todo
                // 获取需要的索引句柄,填充vector ihs
                // lab3 task3 Todo end
            }
        }
        // Delete each rid from record file and index file
        for (auto &rid : rids_) {
            auto rec = fh_->get_record(rid, context_);
            // lab3 task3 Todo
            // Delete from index file
            // Delete from record file
            // lab3 task3 Todo end

            // record a delete operation into the transaction
            RmRecord delete_record{rec->size};
            memcpy(delete_record.data, rec->data, rec->size);
        }
        return nullptr;
    }
    Rid &rid() override { return _abstract_rid; }
};