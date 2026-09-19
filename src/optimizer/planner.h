// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "common/common.h"
#include "common/defs.h"
#include "parser/ast.h"

class Context;
class Plan;
class SelectPlan;
struct AnalyzedQuery;
class SmManager;

class Planner {
private:
    SmManager* sm_manager_;
    std::atomic<bool> enable_sortmerge_{false};  ///< SET enable_sortmerge；服务端全局生效。

    enum class ConditionCoverage {
        None,
        Lhs,
        Rhs,
        Both,
    };

public:
    explicit Planner(SmManager* sm_manager) : sm_manager_(sm_manager) {}

    std::unique_ptr<Plan> do_planner(std::shared_ptr<AnalyzedQuery> query, Context* context);

    /** @brief 设置是否对含等值条件的连接使用排序归并连接。默认关闭，即全部使用嵌套循环连接。 */
    void set_enable_sortmerge(bool enable) noexcept { enable_sortmerge_.store(enable); }
    [[nodiscard]] bool enable_sortmerge() const noexcept { return enable_sortmerge_.load(); }

private:
    // 逻辑优化：改写查询结构。
    std::shared_ptr<AnalyzedQuery> logical_optimization(std::shared_ptr<AnalyzedQuery> query, Context* context);

    // 物理优化：选择扫描、连接和排序算子。
    std::unique_ptr<Plan> physical_optimization(const AnalyzedQuery& query, Context* context);

    std::unique_ptr<SelectPlan> plan_select(std::shared_ptr<AnalyzedQuery> query, Context* context);
    std::unique_ptr<Plan> build_from_plan(const AnalyzedQuery& query);
    std::unique_ptr<Plan> build_join_tree(const std::vector<std::string>& tables,
                                          std::vector<std::unique_ptr<Plan>> scan_plans,
                                          std::vector<Condition> join_conditions);
    std::unique_ptr<Plan> make_scan_plan(const std::string& table, std::vector<Condition> conds) const;
    std::unique_ptr<Plan> choose_join_algorithm(std::unique_ptr<Plan> plan) const;

    bool choose_index(const std::string& table,
                      const std::vector<Condition>& conds,
                      std::vector<std::string>& index_columns) const;

    static std::vector<Condition> take_table_conditions(std::vector<Condition>& conditions, const std::string& table);
    static std::size_t table_index(const std::vector<std::string>& tables, const std::string& table);
    static std::unique_ptr<Plan> take_scan_plan(std::size_t index,
                                                std::vector<bool>& joined,
                                                std::vector<std::unique_ptr<Plan>>& scan_plans);
    static ConditionCoverage attach_join_condition(Condition& condition, Plan& plan);
    static CompOp reverse_comparison(CompOp op);

    static ColType interpret_type(ast::DataType type) {
        switch (type) {
            case ast::DataType::Int:
                return TYPE_INT;
            case ast::DataType::Float:
                return TYPE_FLOAT;
            case ast::DataType::String:
                return TYPE_STRING;
        }
        throw InternalError("Unexpected AST data type");
    }
};
