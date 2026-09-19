// Copyright (c) 2023-2027 Renmin University of China
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
    [[maybe_unused]] SmManager* sm_manager_;
    Planner* planner_;

public:
    Optimizer(SmManager* sm_manager, Planner* planner) : sm_manager_(sm_manager), planner_(planner) {}

    // query 仅只读使用，用 const 引用避免 shared_ptr 拷贝。
    std::unique_ptr<Plan> plan_query(const std::shared_ptr<AnalyzedQuery>& query, Context* context) const {
        switch (query->bound_statement->kind()) {
            case ast::StatementKind::Help:
                return std::make_unique<OtherPlan>(T_Help, std::string());
            case ast::StatementKind::ShowDatabase:
                return std::make_unique<OtherPlan>(T_ShowDatabase, std::string());
            case ast::StatementKind::ShowTables:
                return std::make_unique<OtherPlan>(T_ShowTable, std::string());
            case ast::StatementKind::SetKnob:
                return std::make_unique<SetKnobPlan>(query->bound_knob_name, query->bound_knob_value);
            case ast::StatementKind::DescTable: {
                const auto& desc_table = static_cast<const ast::DescTableStmt&>(*query->bound_statement);
                return std::make_unique<OtherPlan>(T_DescTable, desc_table.tab_name);
            }
            case ast::StatementKind::TxnBegin:
                return std::make_unique<OtherPlan>(T_Transaction_begin, std::string());
            case ast::StatementKind::TxnAbort:
                return std::make_unique<OtherPlan>(T_Transaction_abort, std::string());
            case ast::StatementKind::TxnCommit:
                return std::make_unique<OtherPlan>(T_Transaction_commit, std::string());
            case ast::StatementKind::TxnRollback:
                return std::make_unique<OtherPlan>(T_Transaction_rollback, std::string());
            case ast::StatementKind::CreateTable:
            case ast::StatementKind::DropTable:
            case ast::StatementKind::CreateIndex:
            case ast::StatementKind::DropIndex:
            case ast::StatementKind::Insert:
            case ast::StatementKind::Delete:
            case ast::StatementKind::Update:
            case ast::StatementKind::Select:
                return planner_->do_planner(query, context);
        }
        throw InternalError("Unexpected statement kind");
    }
};
