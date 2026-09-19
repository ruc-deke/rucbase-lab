// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "analyze/analyzed_query.h"
#include "common/common.h"
#include "parser/ast.h"
#include "system/sm_meta.h"

class SmManager;

/** @brief 将 Parser 生成的 AST 绑定为经过语义检查的查询中间结果。 */
class Analyzer {
private:
    SmManager* sm_manager_;

public:
    explicit Analyzer(SmManager* sm_manager) : sm_manager_(sm_manager) {}

    /**
     * @brief 执行表、列和类型绑定，不修改输入 AST。
     * @param statement Parser 生成的语句根节点。
     * @return 绑定后的查询中间结果。
     */
    std::shared_ptr<AnalyzedQuery> analyze(const std::shared_ptr<ast::Statement>& statement);

private:
    void analyze_select(const ast::SelectStmt& statement, AnalyzedQuery& query) const;
    void analyze_update(const ast::UpdateStmt& statement, AnalyzedQuery& query) const;
    void analyze_delete(const ast::DeleteStmt& statement, AnalyzedQuery& query) const;
    void analyze_insert(const ast::InsertStmt& statement, AnalyzedQuery& query) const;
    static void analyze_set_knob(const ast::SetKnobStmt& statement, AnalyzedQuery& query);

    [[nodiscard]] static std::vector<std::string> get_table_names(const std::shared_ptr<ast::FromNode>& from);
    static TabCol check_column(const std::vector<ColMeta>& all_cols, TabCol target);
    [[nodiscard]] std::vector<ColMeta> get_all_cols(const std::vector<std::string>& tab_names) const;
    static void get_clause(const std::shared_ptr<ast::Expr>& ast_predicate, std::vector<Condition>& conds);
    static void append_clause(const std::shared_ptr<ast::Expr>& ast_predicate, std::vector<Condition>& conds);
    void check_clause(const std::vector<ColMeta>& all_cols, std::vector<Condition>& conds) const;
    static Value convert_ast_value(const ast::Value& ast_value);
    static CompOp convert_ast_comp_op(ast::CompOp op);
};
