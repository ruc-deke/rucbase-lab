// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "common/common.h"
#include "common/defs.h"
#include "parser/ast.h"

class Context;
class Plan;
struct AnalyzedQuery;
class SmManager;

class Planner {
   private:
    SmManager *sm_manager_;

   public:
    explicit Planner(SmManager *sm_manager) : sm_manager_(sm_manager) {}

    std::shared_ptr<Plan> do_planner(std::shared_ptr<AnalyzedQuery> query, Context* context);

private:
    // logical_optimization 可能改写并返回 query，故按值接收 shared_ptr。
    std::shared_ptr<AnalyzedQuery> logical_optimization(std::shared_ptr<AnalyzedQuery> query, Context* context);
    // 以下接口只读 AnalyzedQuery，用 const shared_ptr&；plan 需要包装进新节点时再按值 + move。
    std::shared_ptr<Plan> physical_optimization(const std::shared_ptr<AnalyzedQuery>& query, Context* context);

    std::shared_ptr<Plan> make_one_rel(const std::shared_ptr<AnalyzedQuery>& query);

    static std::shared_ptr<Plan> generate_sort_plan(const std::shared_ptr<AnalyzedQuery>& query,
                                                    std::shared_ptr<Plan> plan);

    std::shared_ptr<Plan> generate_select_plan(std::shared_ptr<AnalyzedQuery> query, Context* context);

    // int get_indexNo(std::string tab_name, std::vector<Condition> curr_conds);
    bool get_index_cols(const std::string& tab_name,
                        const std::vector<Condition>& curr_conds,
                        std::vector<std::string>& index_col_names) const;

    static ColType interp_sv_type(ast::SvType sv_type) {
        const std::map<ast::SvType, ColType> m = {
            {ast::SV_TYPE_INT, TYPE_INT}, {ast::SV_TYPE_FLOAT, TYPE_FLOAT}, {ast::SV_TYPE_STRING, TYPE_STRING}};
        return m.at(sv_type);
    }
};
