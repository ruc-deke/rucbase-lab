// Copyright (c) 2023-2027 Renmin University of China
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

typedef enum PlanTag {
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

/** @brief 所有逻辑执行计划的多态基类。 */
class Plan {
public:
    explicit Plan(const PlanTag tag_) : tag(tag_) {}
    virtual ~Plan() = default;

    PlanTag tag;
};

class ScanPlan : public Plan {
public:
    ScanPlan(PlanTag tag,
             SmManager* sm_manager,
             std::string tab_name,
             std::vector<Condition> conds,
             std::vector<std::string> index_col_names)
        : Plan(tag),
          tab_name_(std::move(tab_name)),
          conds_(std::move(conds)),
          index_col_names_(std::move(index_col_names)) {
        const TabMeta& tab = sm_manager->db_.get_table(tab_name_);
        cols_ = tab.cols;
        len_ = cols_.back().offset + cols_.back().len;
        fed_conds_ = conds_;
    }

    std::string tab_name_;
    std::vector<ColMeta> cols_;
    std::vector<Condition> conds_;
    size_t len_;
    std::vector<Condition> fed_conds_;
    std::vector<std::string> index_col_names_;
};

class JoinPlan : public Plan {
public:
    JoinPlan(PlanTag tag, std::unique_ptr<Plan> left, std::unique_ptr<Plan> right, std::vector<Condition> conds)
        : Plan(tag),
          left_(std::move(left)),
          right_(std::move(right)),
          conds_(std::move(conds)),
          type(ast::JoinType::Inner) {}

    std::unique_ptr<Plan> left_;
    std::unique_ptr<Plan> right_;
    std::vector<Condition> conds_;
    ast::JoinType type;
};

class ProjectionPlan : public Plan {
public:
    ProjectionPlan(PlanTag tag, std::unique_ptr<Plan> subplan, std::vector<TabCol> sel_cols)
        : Plan(tag),
          subplan_(std::move(subplan)),
          sel_cols_(std::move(sel_cols)) {}

    std::unique_ptr<Plan> subplan_;
    std::vector<TabCol> sel_cols_;
};

class SortPlan : public Plan {
public:
    SortPlan(PlanTag tag, std::unique_ptr<Plan> subplan, TabCol sel_col, bool is_desc)
        : Plan(tag),
          subplan_(std::move(subplan)),
          sel_col_(std::move(sel_col)),
          is_desc_(is_desc) {}

    std::unique_ptr<Plan> subplan_;
    TabCol sel_col_;
    bool is_desc_;
};

class SelectPlan : public Plan {
public:
    explicit SelectPlan(std::unique_ptr<ProjectionPlan> projection)
        : Plan(T_select),
          projection_(std::move(projection)) {}

    std::unique_ptr<ProjectionPlan> projection_;
};

class InsertPlan : public Plan {
public:
    InsertPlan(std::string table, std::vector<Value> values)
        : Plan(T_Insert),
          table_(std::move(table)),
          values_(std::move(values)) {}

    std::string table_;
    std::vector<Value> values_;
};

class DeletePlan : public Plan {
public:
    DeletePlan(std::unique_ptr<Plan> scan, std::string table, std::vector<Condition> conds)
        : Plan(T_Delete),
          scan_(std::move(scan)),
          table_(std::move(table)),
          conds_(std::move(conds)) {}

    std::unique_ptr<Plan> scan_;
    std::string table_;
    std::vector<Condition> conds_;
};

class UpdatePlan : public Plan {
public:
    UpdatePlan(std::unique_ptr<Plan> scan,
               std::string table,
               std::vector<Condition> conds,
               std::vector<SetClause> set_clauses)
        : Plan(T_Update),
          scan_(std::move(scan)),
          table_(std::move(table)),
          conds_(std::move(conds)),
          set_clauses_(std::move(set_clauses)) {}

    std::unique_ptr<Plan> scan_;
    std::string table_;
    std::vector<Condition> conds_;
    std::vector<SetClause> set_clauses_;
};

class DDLPlan : public Plan {
public:
    DDLPlan(PlanTag tag,
            std::string tab_name,
            std::vector<std::string> col_names,
            std::vector<ColDef> cols,
            bool unique = false)
        : Plan(tag),
          tab_name_(std::move(tab_name)),
          tab_col_names_(std::move(col_names)),
          cols_(std::move(cols)),
          unique_(unique) {}

    std::string tab_name_;
    std::vector<std::string> tab_col_names_;
    std::vector<ColDef> cols_;
    bool unique_{false};
};

class OtherPlan : public Plan {
public:
    OtherPlan(PlanTag tag, std::string tab_name) : Plan(tag), tab_name_(std::move(tab_name)) {}

    std::string tab_name_;
};
