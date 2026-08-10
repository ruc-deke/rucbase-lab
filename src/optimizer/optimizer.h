// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <memory>
#include <string>

#include "analyze/analyzed_query.h"
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
    std::shared_ptr<Plan> plan_query(const std::shared_ptr<AnalyzedQuery>& query, Context* context) const {
        if (std::dynamic_pointer_cast<const ast::Help>(query->bound_statement)) {
            // help;
            return std::make_shared<OtherPlan>(T_Help, std::string());
        } else if (std::dynamic_pointer_cast<const ast::ShowDatabase>(query->bound_statement)) {
            // show database;
            return std::make_shared<OtherPlan>(T_ShowDatabase, std::string());
        } else if (std::dynamic_pointer_cast<const ast::ShowTables>(query->bound_statement)) {
            // show tables;
            return std::make_shared<OtherPlan>(T_ShowTable, std::string());
        } else if (const auto desc_table = std::dynamic_pointer_cast<const ast::DescTable>(query->bound_statement)) {
            // desc table;
            return std::make_shared<OtherPlan>(T_DescTable, desc_table->tab_name);
        } else if (std::dynamic_pointer_cast<const ast::TxnBegin>(query->bound_statement)) {
            // begin;
            return std::make_shared<OtherPlan>(T_Transaction_begin, std::string());
        } else if (std::dynamic_pointer_cast<const ast::TxnAbort>(query->bound_statement)) {
            // abort;
            return std::make_shared<OtherPlan>(T_Transaction_abort, std::string());
        } else if (std::dynamic_pointer_cast<const ast::TxnCommit>(query->bound_statement)) {
            // commit;
            return std::make_shared<OtherPlan>(T_Transaction_commit, std::string());
        } else if (std::dynamic_pointer_cast<const ast::TxnRollback>(query->bound_statement)) {
            // rollback;
            return std::make_shared<OtherPlan>(T_Transaction_rollback, std::string());
        } else {
            return planner_->do_planner(query, context);
        }
    }
};
