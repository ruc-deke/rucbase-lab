// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <map>

#include "errors.h"
#include "execution/execution.h"
#include "parser/parser.h"
#include "system/sm.h"
#include "common/context.h"
#include "transaction/transaction_manager.h"
#include "planner.h"
#include "plan.h"

class Optimizer {
   private:
    [[maybe_unused]] SmManager *sm_manager_;
    Planner *planner_;

   public:
    Optimizer(SmManager *sm_manager,  Planner *planner) 
        : sm_manager_(sm_manager),  planner_(planner)
        {}
    
    std::shared_ptr<Plan> plan_query(std::shared_ptr<Query> query, Context *context) {
        if (auto x = std::dynamic_pointer_cast<ast::Help>(query->parse)) {
            // help;
            return std::make_shared<OtherPlan>(T_Help, std::string());
        } else if (auto x = std::dynamic_pointer_cast<ast::ShowTables>(query->parse)) {
            // show tables;
            return std::make_shared<OtherPlan>(T_ShowTable, std::string());
        } else if (auto x = std::dynamic_pointer_cast<ast::DescTable>(query->parse)) {
            // desc table;
            return std::make_shared<OtherPlan>(T_DescTable, x->tab_name);
        } else if (auto x = std::dynamic_pointer_cast<ast::TxnBegin>(query->parse)) {
            // begin;
            return std::make_shared<OtherPlan>(T_Transaction_begin, std::string());
        } else if (auto x = std::dynamic_pointer_cast<ast::TxnAbort>(query->parse)) {
            // abort;
            return std::make_shared<OtherPlan>(T_Transaction_abort, std::string());
        } else if (auto x = std::dynamic_pointer_cast<ast::TxnCommit>(query->parse)) {
            // commit;
            return std::make_shared<OtherPlan>(T_Transaction_commit, std::string());
        } else if (auto x = std::dynamic_pointer_cast<ast::TxnRollback>(query->parse)) {
            // rollback;
            return std::make_shared<OtherPlan>(T_Transaction_rollback, std::string());
        } else {
            return planner_->do_planner(query, context);
        }
    }

};
