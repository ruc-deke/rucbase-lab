// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "analyze/analyzer.h"

#include <algorithm>
#include <utility>

#include "system/sm_manager.h"

/**
 * @brief 按 AST 根节点分派语义分析，并保留只读原始语句供下游识别类型。
 */
std::shared_ptr<AnalyzedQuery> Analyzer::analyze(const std::shared_ptr<ast::Statement>& statement) {
    if (statement == nullptr) {
        throw InternalError("Analyzer received a null AST statement");
    }

    // AnalyzedQuery 持有只读 AST；DML 的绑定结果写在 query 的其它字段里。
    auto query = std::make_shared<AnalyzedQuery>(statement);

    // 仅对需要列绑定/类型检查的 DML 做完整分析；DDL 与辅助语句交给 Planner 读 AST。
    switch (statement->kind()) {
        case ast::StatementKind::Select:
            analyze_select(static_cast<const ast::SelectStmt&>(*statement), *query);
            break;
        case ast::StatementKind::Update:
            analyze_update(static_cast<const ast::UpdateStmt&>(*statement), *query);
            break;
        case ast::StatementKind::Delete:
            analyze_delete(static_cast<const ast::DeleteStmt&>(*statement), *query);
            break;
        case ast::StatementKind::Insert:
            analyze_insert(static_cast<const ast::InsertStmt&>(*statement), *query);
            break;
        case ast::StatementKind::Help:
        case ast::StatementKind::ShowTables:
        case ast::StatementKind::ShowDatabase:
        case ast::StatementKind::TxnBegin:
        case ast::StatementKind::TxnCommit:
        case ast::StatementKind::TxnAbort:
        case ast::StatementKind::TxnRollback:
        case ast::StatementKind::CreateTable:
        case ast::StatementKind::DropTable:
        case ast::StatementKind::DescTable:
        case ast::StatementKind::CreateIndex:
        case ast::StatementKind::DropIndex:
            // 无列绑定工作；bound_statement 已足够下游分派。
            break;
    }
    return query;
}

/**
 * @brief 在 FROM 可见范围内绑定投影列、ORDER BY 和 WHERE。
 *
 * 同一份 all_cols 在三个绑定步骤间复用，避免重复查询目录。
 */
void Analyzer::analyze_select(const ast::SelectStmt& statement, AnalyzedQuery& query) const {
    if (statement.from == nullptr) {
        throw InternalError("SELECT statement has no FROM source");
    }

    // 1) 展开 FROM 树为表名列表（同时拒绝别名、外连接等尚未支持的语法）。
    query.bound_tables = get_table_names(statement.from);

    // 2) 收集可见列元数据；get_table 也会验证表是否存在。
    const auto all_cols = get_all_cols(query.bound_tables);

    // 3) 绑定 SELECT 投影：目前只接受列引用和 * / table.*。
    if (statement.select_items.empty()) {
        throw InternalError("SELECT statement has an empty projection list");
    }
    query.bound_cols.reserve(statement.select_items.size());
    for (const auto& item : statement.select_items) {
        if (item == nullptr) {
            throw InternalError("SELECT projection contains a null expression");
        }
        switch (item->kind()) {
            case ast::ExprKind::Column: {
                // 先记录 AST 中的表/列名，真正的存在性与消歧义在 check_column 完成。
                const auto& column = static_cast<const ast::Col&>(*item);
                query.bound_cols.push_back({.tab_name = column.tab_name, .col_name = column.col_name});
                break;
            }
            case ast::ExprKind::Star: {
                // * 展开为 all_cols；table.* 先确认限定表在 FROM 中，再过滤同表列。
                const auto& star = static_cast<const ast::StarExpr&>(*item);
                if (!star.tab_name.empty() &&
                    std::ranges::find(query.bound_tables, star.tab_name) == query.bound_tables.end()) {
                    throw TableNotFoundError(star.tab_name);
                }
                for (const auto& col : all_cols) {
                    if (star.tab_name.empty() || col.tab_name == star.tab_name) {
                        query.bound_cols.push_back({.tab_name = col.tab_name, .col_name = col.name});
                    }
                }
                break;
            }
            case ast::ExprKind::IntLiteral:
            case ast::ExprKind::FloatLiteral:
            case ast::ExprKind::StringLiteral:
            case ast::ExprKind::Binary:
            case ast::ExprKind::Logical:
            case ast::ExprKind::Unary:
            case ast::ExprKind::FunctionCall:
            case ast::ExprKind::Subquery:
                // Parser 可能生成更宽的表达式节点，执行层尚未支持。
                throw NotImplementedError("This SELECT item expression is not supported yet");
        }
    }

    // 为投影列补全/校验表名（未限定列在多表时可能歧义）。
    for (auto& col : query.bound_cols) {
        col = check_column(all_cols, std::move(col));
    }

    // 4) SELECT 尾部子句：高级子句显式拒绝；ORDER BY 仅支持单列。
    if (!statement.group_by.empty() || statement.having != nullptr || statement.limit.has_value()) {
        throw NotImplementedError("GROUP BY, HAVING, and LIMIT are not supported yet");
    }
    if (statement.order_by.size() > 1) {
        throw NotImplementedError("Multiple ORDER BY items are not supported yet");
    }
    if (!statement.order_by.empty()) {
        const auto& order_by = statement.order_by.front();
        if (order_by == nullptr || order_by->expression == nullptr) {
            throw InternalError("ORDER BY contains a null expression");
        }
        // 排序键必须是列，不支持表达式排序。
        if (order_by->expression->kind() != ast::ExprKind::Column) {
            throw NotImplementedError("ORDER BY expressions are not supported yet");
        }
        const auto& order_column = static_cast<const ast::Col&>(*order_by->expression);
        TabCol order_col{.tab_name = order_column.tab_name, .col_name = order_column.col_name};
        query.bound_order_col = check_column(all_cols, std::move(order_col));
        query.bound_order_desc = order_by->direction == ast::OrderByDir::Desc;
    }

    // 5) WHERE：先展平为 Condition 列表，再做列绑定与类型检查。
    get_clause(statement.where, query.bound_conds);
    check_clause(all_cols, query.bound_conds);
}

/**
 * @brief 按源码顺序从递归 FROM 树中收集命名表。
 *
 * 递归 JOIN 结构仍完整保留在只读 AST 中。当前 Planner 只需要表名列表，
 * 因此 Analyzer 不再复制一棵尚无消费者的绑定 JOIN 树。
 */
std::vector<std::string> Analyzer::get_table_names(const std::shared_ptr<ast::FromNode>& from) {
    if (from == nullptr) {
        throw InternalError("FROM tree contains a null node");
    }

    switch (from->kind()) {
        case ast::FromNodeKind::Table: {
            const auto& table = static_cast<const ast::TableRef&>(*from);
            // 别名会改变列限定名解析规则，当前绑定逻辑尚未接入。
            if (!table.alias.empty()) {
                throw NotImplementedError("Table aliases are not supported yet");
            }
            // TableRef::source 也可能是派生表子查询；Lab 子集只接受真实表名。
            const auto* table_name = std::get_if<std::string>(&table.source);
            if (table_name == nullptr) {
                throw NotImplementedError("Derived-table subqueries are not supported yet");
            }
            return {*table_name};
        }
        case ast::FromNodeKind::Join: {
            const auto& join = static_cast<const ast::JoinNode&>(*from);
            // 逗号连接记为 Cross，显式 JOIN 记为 Inner；外连接与 ON 条件尚未实现。
            if (join.type != ast::JoinType::Cross && join.type != ast::JoinType::Inner) {
                throw NotImplementedError("Outer joins are not supported yet");
            }
            if (join.condition != nullptr) {
                throw NotImplementedError("JOIN ON predicates are not supported yet");
            }
            // 左深递归展开：保持 FROM 从左到右的表顺序，供后续谓词下推使用。
            auto table_names = get_table_names(join.left);
            auto right_table_names = get_table_names(join.right);
            table_names.insert(table_names.end(), right_table_names.begin(), right_table_names.end());
            return table_names;
        }
    }
    throw InternalError("Unexpected FROM node kind");
}

void Analyzer::analyze_update(const ast::UpdateStmt& statement, AnalyzedQuery& query) const {
    // 目标表必须存在；列可见范围限定为该表。
    const TabMeta& table = sm_manager_->db_.get_table(statement.tab_name);
    query.bound_target_table = table.name;
    query.bound_set_clauses.reserve(statement.set_clauses.size());

    for (const auto& ast_set_clause : statement.set_clauses) {
        if (ast_set_clause == nullptr || ast_set_clause->target_column == nullptr ||
            ast_set_clause->assigned_expr == nullptr) {
            throw InternalError("UPDATE SET contains a null AST node");
        }

        // 左侧：绑定赋值目标列（允许 table.col 或未限定 col）。
        TabCol target{
            .tab_name = ast_set_clause->target_column->tab_name,
            .col_name = ast_set_clause->target_column->col_name,
        };
        target = check_column(table.cols, std::move(target));

        // 右侧：目前只接受字面量；函数/列表达式等由 Parser 解析后在此明确拒绝。
        if (ast_set_clause->assigned_expr->kind() != ast::ExprKind::IntLiteral &&
            ast_set_clause->assigned_expr->kind() != ast::ExprKind::FloatLiteral &&
            ast_set_clause->assigned_expr->kind() != ast::ExprKind::StringLiteral) {
            throw NotImplementedError("UPDATE SET expressions are not supported yet");
        }

        SetClause set_clause{
            .lhs = std::move(target),
            .rhs = convert_ast_value(static_cast<const ast::Value&>(*ast_set_clause->assigned_expr)),
        };
        // 类型一致后，按目标列物理长度预编码 rhs（定长字符串等）。
        const auto column = table.get_col(set_clause.lhs.col_name);
        if (column->type != set_clause.rhs.type) {
            throw IncompatibleTypeError(coltype2str(column->type), coltype2str(set_clause.rhs.type));
        }
        set_clause.rhs.init_raw(column->len);
        query.bound_set_clauses.emplace_back(std::move(set_clause));
    }

    // WHERE 与 SELECT 共用同一套条件展平与检查逻辑。
    get_clause(statement.where, query.bound_conds);
    check_clause(table.cols, query.bound_conds);
}

void Analyzer::analyze_delete(const ast::DeleteStmt& statement, AnalyzedQuery& query) const {
    // DELETE 只需确认目标表，并把 WHERE 绑定到该表的列空间。
    const TabMeta& table = sm_manager_->db_.get_table(statement.tab_name);
    query.bound_target_table = table.name;
    get_clause(statement.where, query.bound_conds);
    check_clause(table.cols, query.bound_conds);
}

/** @brief 按表定义的列顺序检查 INSERT 值，并预先编码定长存储形式。 */
void Analyzer::analyze_insert(const ast::InsertStmt& statement, AnalyzedQuery& query) const {
    const TabMeta& table = sm_manager_->db_.get_table(statement.tab_name);
    query.bound_target_table = table.name;

    // INSERT 值个数必须与表列数一一对应（按声明顺序）。
    if (statement.values.size() != table.cols.size()) {
        throw InvalidValueCountError();
    }

    query.bound_values.reserve(statement.values.size());
    for (size_t i = 0; i < statement.values.size(); ++i) {
        if (statement.values[i] == nullptr) {
            throw InternalError("INSERT contains a null value node");
        }
        Value value = convert_ast_value(*statement.values[i]);
        const ColMeta& column = table.cols[i];
        if (value.type != column.type) {
            throw IncompatibleTypeError(coltype2str(column.type), coltype2str(value.type));
        }
        // 提前编码为执行层可直接写入的定长 raw 形式。
        value.init_raw(column.len);
        query.bound_values.emplace_back(std::move(value));
    }
}

/**
 * @brief 验证已限定列，或在给定可见列中解析未限定列。
 * @throws AmbiguousColumnError 未限定列名在多张表中匹配。
 * @throws ColumnNotFoundError 列不存在或不在当前语句的可见范围内。
 */
TabCol Analyzer::check_column(const std::vector<ColMeta>& all_cols, TabCol target) {
    if (target.tab_name.empty()) {
        // 未限定列：在可见列中唯一匹配到一张表，才能补全 tab_name。
        std::string tab_name;
        for (const auto& col : all_cols) {
            if (col.name == target.col_name) {
                if (!tab_name.empty()) {
                    throw AmbiguousColumnError(target.col_name);
                }
                tab_name = col.tab_name;
            }
        }
        if (tab_name.empty()) {
            throw ColumnNotFoundError(target.col_name);
        }
        target.tab_name = std::move(tab_name);
    } else {
        // 已限定列：表名 + 列名必须同时命中可见范围。
        const auto col = std::ranges::find_if(all_cols, [&](const ColMeta& candidate) {
            return candidate.tab_name == target.tab_name && candidate.name == target.col_name;
        });
        if (col == all_cols.end()) {
            throw ColumnNotFoundError(target.tab_name + '.' + target.col_name);
        }
    }
    return target;
}

/** @brief 按 FROM 顺序收集列元数据；查表过程同时验证表是否存在。 */
std::vector<ColMeta> Analyzer::get_all_cols(const std::vector<std::string>& tab_names) const {
    std::vector<ColMeta> all_cols;
    for (const auto& tab_name : tab_names) {
        // get_table 在表不存在时抛错，因此这里兼做 FROM 表存在性检查。
        const auto& cols = sm_manager_->db_.get_table(tab_name).cols;
        all_cols.insert(all_cols.end(), cols.begin(), cols.end());
    }
    return all_cols;
}

/** @brief 将当前支持的合取谓词树展平为 Condition，绑定和类型检查由 check_clause 完成。 */
void Analyzer::get_clause(const std::shared_ptr<ast::Expr>& ast_predicate, std::vector<Condition>& conds) {
    conds.clear();
    // WHERE/ON 缺省时谓词为空，下游按“无过滤”处理。
    if (ast_predicate != nullptr) {
        append_clause(ast_predicate, conds);
    }
}

void Analyzer::append_clause(const std::shared_ptr<ast::Expr>& ast_predicate, std::vector<Condition>& conds) {
    if (ast_predicate == nullptr) {
        throw InternalError("Predicate tree contains a null expression");
    }
    switch (ast_predicate->kind()) {
        case ast::ExprKind::Logical: {
            // 只支持 AND：把左、右子树继续展平，最终得到合取的 Condition 向量。
            const auto& logical = static_cast<const ast::LogicalExpr&>(*ast_predicate);
            if (logical.op != ast::LogicalOp::And) {
                throw NotImplementedError("OR predicates are not supported yet");
            }
            append_clause(logical.lhs, conds);
            append_clause(logical.rhs, conds);
            return;
        }
        case ast::ExprKind::Unary:
            // NOT 会改变谓词极性，当前执行层只吃合取比较条件。
            throw NotImplementedError("NOT predicates are not supported yet");
        case ast::ExprKind::Binary: {
            // 一个比较谓词对应一条 Condition：lhs 必须是列，rhs 可以是字面量或列。
            const auto& comparison = static_cast<const ast::BinaryExpr&>(*ast_predicate);
            if (comparison.lhs == nullptr || comparison.rhs == nullptr) {
                throw InternalError("Comparison contains a null operand");
            }
            Condition cond{};
            cond.lhs_col = {.tab_name = comparison.lhs->tab_name, .col_name = comparison.lhs->col_name};
            cond.op = convert_ast_comp_op(comparison.op);
            switch (comparison.rhs->kind()) {
                case ast::ExprKind::IntLiteral:
                case ast::ExprKind::FloatLiteral:
                case ast::ExprKind::StringLiteral:
                    // col op value
                    cond.is_rhs_val = true;
                    cond.rhs_val = convert_ast_value(static_cast<const ast::Value&>(*comparison.rhs));
                    break;
                case ast::ExprKind::Column: {
                    // col op col（连接条件或同表列比较）
                    const auto& rhs_col = static_cast<const ast::Col&>(*comparison.rhs);
                    cond.is_rhs_val = false;
                    cond.rhs_col = {.tab_name = rhs_col.tab_name, .col_name = rhs_col.col_name};
                    break;
                }
                case ast::ExprKind::Star:
                case ast::ExprKind::Binary:
                case ast::ExprKind::Logical:
                case ast::ExprKind::Unary:
                case ast::ExprKind::FunctionCall:
                case ast::ExprKind::Subquery:
                    throw NotImplementedError("This comparison right-hand side is not supported yet");
            }
            conds.emplace_back(std::move(cond));
            return;
        }
        case ast::ExprKind::IntLiteral:
        case ast::ExprKind::FloatLiteral:
        case ast::ExprKind::StringLiteral:
        case ast::ExprKind::Column:
        case ast::ExprKind::Star:
        case ast::ExprKind::FunctionCall:
        case ast::ExprKind::Subquery:
            // 谓词顶层必须是比较或 AND 树，不能单独出现列/字面量。
            throw NotImplementedError("This predicate expression is not supported yet");
    }
    throw InternalError("Unexpected predicate expression kind");
}

/**
 * @brief 先将条件两侧列绑定到可见表，再检查类型并编码字面量。
 * @pre all_cols 必须完整覆盖当前语句可见的表。
 */
void Analyzer::check_clause(const std::vector<ColMeta>& all_cols, std::vector<Condition>& conds) const {
    for (auto& cond : conds) {
        // 先做列绑定，保证后续取 ColMeta 时表名、列名都已确定。
        cond.lhs_col = check_column(all_cols, std::move(cond.lhs_col));
        if (!cond.is_rhs_val) {
            cond.rhs_col = check_column(all_cols, std::move(cond.rhs_col));
        }

        // 比较两侧类型必须一致（当前不做隐式类型转换）。
        const auto lhs_col = sm_manager_->db_.get_table(cond.lhs_col.tab_name).get_col(cond.lhs_col.col_name);
        ColType rhs_type;
        if (cond.is_rhs_val) {
            rhs_type = cond.rhs_val.type;
        } else {
            rhs_type = sm_manager_->db_.get_table(cond.rhs_col.tab_name).get_col(cond.rhs_col.col_name)->type;
        }
        if (lhs_col->type != rhs_type) {
            throw IncompatibleTypeError(coltype2str(lhs_col->type), coltype2str(rhs_type));
        }
        // 字面量按左侧列长度编码，便于执行时直接 memcmp / 写入。
        if (cond.is_rhs_val) {
            cond.rhs_val.init_raw(lhs_col->len);
        }
    }
}

Value Analyzer::convert_ast_value(const ast::Value& ast_value) {
    // 将 Parser 的字面量 AST 节点转换为执行层共用的 Value。
    Value value;
    switch (ast_value.kind()) {
        case ast::ExprKind::IntLiteral:
            value.set_int(static_cast<const ast::IntLit&>(ast_value).val);
            break;
        case ast::ExprKind::FloatLiteral:
            value.set_float(static_cast<const ast::FloatLit&>(ast_value).val);
            break;
        case ast::ExprKind::StringLiteral:
            value.set_str(static_cast<const ast::StringLit&>(ast_value).val);
            break;
        case ast::ExprKind::Column:
        case ast::ExprKind::Star:
        case ast::ExprKind::Binary:
        case ast::ExprKind::Logical:
        case ast::ExprKind::Unary:
        case ast::ExprKind::FunctionCall:
        case ast::ExprKind::Subquery:
            // Value 基类只应承载字面量；其它表达式不应走到这里。
            throw InternalError("Non-literal expression stored as an AST value");
    }
    return value;
}

CompOp Analyzer::convert_ast_comp_op(const ast::CompOp op) {
    // AST 比较符 → 执行层 Condition 使用的 CompOp。
    switch (op) {
        case ast::CompOp::Eq:
            return OP_EQ;
        case ast::CompOp::Ne:
            return OP_NE;
        case ast::CompOp::Lt:
            return OP_LT;
        case ast::CompOp::Gt:
            return OP_GT;
        case ast::CompOp::Le:
            return OP_LE;
        case ast::CompOp::Ge:
            return OP_GE;
    }
    throw InternalError("Unexpected comparison operator");
}
