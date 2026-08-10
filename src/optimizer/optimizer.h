// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <memory>
#include <string>

#include "analyze/analyze.h"
#include "parser/ast.h"
#include "plan.h"
#include "planner.h"

class Context;
class SmManager;

class Optimizer {
   private:
    [[maybe_unused]] SmManager *sm_manager_;
    Planner *planner_;

   public:
    Optimizer(SmManager *sm_manager,  Planner *planner) 
        : sm_manager_(sm_manager),  planner_(planner)
        {}
    
    // query 仅只读使用（dynamic_pointer_cast / 读字段），用 const 引用避免 shared_ptr 拷贝。
    std::shared_ptr<Plan> plan_query(const std::shared_ptr<Query> &query, Context *context) const {
        if (std::dynamic_pointer_cast<ast::Help>(query->parse)) {
            // help;
            return std::make_shared<OtherPlan>(T_Help, std::string());
        } else if (std::dynamic_pointer_cast<ast::ShowDatabase>(query->parse)) {
            // show database;
            return std::make_shared<OtherPlan>(T_ShowDatabase, std::string());
        } else if (std::dynamic_pointer_cast<ast::ShowTables>(query->parse)) {
            // show tables;
            return std::make_shared<OtherPlan>(T_ShowTable, std::string());
        } else if (const auto desc_table = std::dynamic_pointer_cast<ast::DescTable>(query->parse)) {
            // desc table;
            return std::make_shared<OtherPlan>(T_DescTable, desc_table->tab_name);
        } else if (std::dynamic_pointer_cast<ast::TxnBegin>(query->parse)) {
            // begin;
            return std::make_shared<OtherPlan>(T_Transaction_begin, std::string());
        } else if (std::dynamic_pointer_cast<ast::TxnAbort>(query->parse)) {
            // abort;
            return std::make_shared<OtherPlan>(T_Transaction_abort, std::string());
        } else if (std::dynamic_pointer_cast<ast::TxnCommit>(query->parse)) {
            // commit;
            return std::make_shared<OtherPlan>(T_Transaction_commit, std::string());
        } else if (std::dynamic_pointer_cast<ast::TxnRollback>(query->parse)) {
            // rollback;
            return std::make_shared<OtherPlan>(T_Transaction_rollback, std::string());
        } else {
            return planner_->do_planner(query, context);
        }
    }

};
