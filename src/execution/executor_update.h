// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/common.h"
#include "common/defs.h"
#include "common/errors.h"
#include "executor_abstract.h"
#include "record/rm_defs.h"
#include "record/rm_file_handle.h"
#include "system/sm_manager.h"
#include "system/sm_meta.h"

class UpdateExecutor : public AbstractExecutor {
private:
    TabMeta tab_;
    std::vector<Condition> conds_;
    RmFileHandle* fh_;
    std::vector<Rid> rids_;
    std::string tab_name_;
    std::vector<SetClause> set_clauses_;
    SmManager* sm_manager_;

public:
    UpdateExecutor(SmManager* sm_manager,
                   const std::string& tab_name,
                   std::vector<SetClause> set_clauses,
                   std::vector<Condition> conds,
                   std::vector<Rid> rids,
                   Context* context) {
        sm_manager_ = sm_manager;
        tab_name_ = tab_name;
        set_clauses_ = std::move(set_clauses);
        tab_ = sm_manager_->db_.get_table(tab_name);
        fh_ = sm_manager_->fhs_.at(tab_name).get();
        conds_ = std::move(conds);
        rids_ = std::move(rids);
        context_ = context;
    }
    /**
     * @brief 按 rids_ 逐条更新记录，并维护表上的全部索引。
     * @throws DuplicateKeyError 新值与某个唯一索引中已有的 key 冲突。抛出前，发生冲突的这一行
     *         （记录本身以及它在所有索引中的项）必须恢复原状。
     * @note 把一行的唯一键更新为它原来的值不算冲突。
     */
    std::unique_ptr<RmRecord> next() override { throw NotImplementedError("UpdateExecutor::next"); }

    Rid& rid() override { return abstract_rid_; }
};
