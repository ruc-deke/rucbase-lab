// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/common.h"
#include "parser/ast.h"
#include "system/sm_defs.h"
#include "system/sm_manager.h"
#include "system/sm_meta.h"

typedef enum PlanTag{
    T_Invalid = 1,
    T_Help,
    T_ShowDatabase,
    T_ShowTable,
    T_DescTable,
    T_CreateTable,
    T_DropTable,
    T_CreateIndex,
    T_DropIndex,
    T_Insert,
    T_Update,
    T_Delete,
    T_select,
    T_Transaction_begin,
    T_Transaction_commit,
    T_Transaction_abort,
    T_Transaction_rollback,
    T_SeqScan,
    T_IndexScan,
    T_NestLoop,
    T_Sort,
    T_Projection
} PlanTag;

// 查询执行计划
class Plan
{
public:
    PlanTag tag;
    virtual ~Plan() = default;
};

class ScanPlan : public Plan
{
    public:
        ScanPlan(PlanTag tag, SmManager *sm_manager, std::string tab_name, std::vector<Condition> conds, std::vector<std::string> index_col_names)
        {
            Plan::tag = tag;
            tab_name_ = std::move(tab_name);
            conds_ = std::move(conds);
            TabMeta &tab = sm_manager->db_.get_table(tab_name_);
            cols_ = tab.cols;
            len_ = cols_.back().offset + cols_.back().len;
            fed_conds_ = conds_;
            index_col_names_ = std::move(index_col_names);
        
        }
        ~ScanPlan() override = default;
        // 以下变量同ScanExecutor中的变量
        std::string tab_name_;                     
        std::vector<ColMeta> cols_;                
        std::vector<Condition> conds_;             
        size_t len_;                               
        std::vector<Condition> fed_conds_;
        std::vector<std::string> index_col_names_;
    
};

class JoinPlan : public Plan
{
    public:
        JoinPlan(PlanTag tag, std::shared_ptr<Plan> left, std::shared_ptr<Plan> right, std::vector<Condition> conds)
        {
            Plan::tag = tag;
            left_ = std::move(left);
            right_ = std::move(right);
            conds_ = std::move(conds);
            type = ast::JoinType::Inner;
        }
        ~JoinPlan() override = default;
        // 左节点
        std::shared_ptr<Plan> left_;
        // 右节点
        std::shared_ptr<Plan> right_;
        // 连接条件
        std::vector<Condition> conds_;
        // future TODO: 后续可以支持的连接类型
        ast::JoinType type;
        
};

class ProjectionPlan : public Plan
{
    public:
        ProjectionPlan(PlanTag tag, std::shared_ptr<Plan> subplan, std::vector<TabCol> sel_cols)
        {
            Plan::tag = tag;
            subplan_ = std::move(subplan);
            sel_cols_ = std::move(sel_cols);
        }
        ~ProjectionPlan() override = default;
        std::shared_ptr<Plan> subplan_;
        std::vector<TabCol> sel_cols_;
        
};

class SortPlan : public Plan
{
    public:
        SortPlan(PlanTag tag, std::shared_ptr<Plan> subplan, TabCol sel_col, bool is_desc)
        {
            Plan::tag = tag;
            subplan_ = std::move(subplan);
            sel_col_ = std::move(sel_col);
            is_desc_ = is_desc;
        }
        ~SortPlan() override = default;
        std::shared_ptr<Plan> subplan_;
        TabCol sel_col_;
        bool is_desc_;
        
};

// dml语句，包括insert; delete; update; select语句　
class DMLPlan : public Plan
{
    public:
        DMLPlan(PlanTag tag, std::shared_ptr<Plan> subplan,std::string tab_name,
                std::vector<Value> values, std::vector<Condition> conds,
                std::vector<SetClause> set_clauses)
        {
            Plan::tag = tag;
            subplan_ = std::move(subplan);
            tab_name_ = std::move(tab_name);
            values_ = std::move(values);
            conds_ = std::move(conds);
            set_clauses_ = std::move(set_clauses);
        }
        ~DMLPlan() override = default;
        std::shared_ptr<Plan> subplan_;
        std::string tab_name_;
        std::vector<Value> values_;
        std::vector<Condition> conds_;
        std::vector<SetClause> set_clauses_;
};

// ddl语句, 包括create/drop table; create/drop index;
class DDLPlan : public Plan
{
    public:
        DDLPlan(PlanTag tag, std::string tab_name, std::vector<std::string> col_names, std::vector<ColDef> cols)
        {
            Plan::tag = tag;
            tab_name_ = std::move(tab_name);
            cols_ = std::move(cols);
            tab_col_names_ = std::move(col_names);
        }
        ~DDLPlan() override = default;
        std::string tab_name_;
        std::vector<std::string> tab_col_names_;
        std::vector<ColDef> cols_;
};

// help; show tables; desc tables; begin; abort; commit; rollback语句对应的plan
class OtherPlan : public Plan
{
    public:
        OtherPlan(PlanTag tag, std::string tab_name)
        {
            Plan::tag = tag;
            tab_name_ = std::move(tab_name);            
        }
        ~OtherPlan() override = default;
        std::string tab_name_;
};

class plannerInfo{
    public:
    std::shared_ptr<ast::SelectStmt> parse;
    std::vector<Condition> where_conds;
    std::vector<TabCol> sel_cols;
    std::shared_ptr<Plan> plan;
    std::vector<std::shared_ptr<Plan>> table_scan_executors;
    std::vector<SetClause> set_clauses;
    explicit plannerInfo(std::shared_ptr<ast::SelectStmt> parse_):parse(std::move(parse_)){}

};
