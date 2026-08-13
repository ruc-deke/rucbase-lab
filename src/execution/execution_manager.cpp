// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "execution_manager.h"

#include <algorithm>
#include <span>

#include "common/config.h"
#include "common/context.h"
#include "common/wire_result.h"
#include "executor_delete.h"
#include "executor_index_scan.h"
#include "executor_insert.h"
#include "executor_nestedloop_join.h"
#include "executor_projection.h"
#include "executor_seq_scan.h"
#include "executor_update.h"
#include "optimizer/plan.h"
#include "system/sm_manager.h"
#include "transaction/transaction.h"
#include "transaction/transaction_manager.h"

constexpr char help_info[] =
    "Supported SQL syntax:\n"
    "  command ;\n"
    "command:\n"
    "  SHOW DATABASE\n"
    "  SHOW TABLES\n"
    "  DESC table_name\n"
    "  CREATE TABLE table_name (column_name type [, column_name type ...])\n"
    "  DROP TABLE table_name\n"
    "  CREATE [UNIQUE] INDEX table_name (column_name)\n"
    "  DROP INDEX table_name (column_name)\n"
    "  INSERT INTO table_name VALUES (value [, value ...])\n"
    "  DELETE FROM table_name [WHERE where_clause]\n"
    "  UPDATE table_name SET column_name = value [, column_name = value ...] [WHERE where_clause]\n"
    "  SELECT selector FROM table_name [WHERE where_clause]\n"
    "type:\n"
    "  {INT | FLOAT | CHAR(n)}\n"
    "where_clause:\n"
    "  condition [AND condition ...]\n"
    "condition:\n"
    "  column op {column | value}\n"
    "column:\n"
    "  [table_name.]column_name\n"
    "op:\n"
    "  {= | <> | < | > | <= | >=}\n"
    "selector:\n"
    "  {* | column [, column ...]}\n";

// 主要负责执行DDL语句
void QlManager::run_mutli_query(const Plan& plan, Context* context) const {
    if (const auto* ddl_plan = dynamic_cast<const DDLPlan*>(&plan)) {
        switch (ddl_plan->tag) {
            case T_CreateTable: {
                sm_manager_->create_table(ddl_plan->tab_name_, ddl_plan->cols_, context);
                break;
            }
            case T_DropTable: {
                sm_manager_->drop_table(ddl_plan->tab_name_, context);
                break;
            }
            case T_CreateIndex: {
                sm_manager_->create_index(ddl_plan->tab_name_, ddl_plan->tab_col_names_, ddl_plan->unique_, context);
                break;
            }
            case T_DropIndex: {
                sm_manager_->drop_index(ddl_plan->tab_name_, ddl_plan->tab_col_names_, context);
                break;
            }
            default:
                throw InternalError("Unexpected ddl type");
                break;
        }
    }
}

// 执行 help; show database; show tables; desc table; begin; commit; abort; 语句。
void QlManager::run_cmd_utility(const Plan& plan, txn_id_t* txn_id, Context* context) {
    if (const auto* utility_plan = dynamic_cast<const OtherPlan*>(&plan)) {
        switch (utility_plan->tag) {
            case T_Help: {
                context->result().set_raw_text("Help", help_info);
                break;
            }
            case T_ShowDatabase: {
                sm_manager_->show_database(context);
                break;
            }
            case T_ShowTable: {
                sm_manager_->show_tables(context);
                break;
            }
            case T_DescTable: {
                sm_manager_->desc_table(utility_plan->tab_name_, context);
                break;
            }
            case T_Transaction_begin: {
                if (context->transaction() == nullptr) {
                    throw NotImplementedError("Lab 4 transaction control");
                }
                context->transaction()->set_txn_mode(true);
                break;
            }
            case T_Transaction_commit: {
                if (context->transaction() == nullptr) {
                    throw NotImplementedError("Lab 4 transaction control");
                }
                context->set_transaction(txn_mgr_->get_transaction(*txn_id));
                txn_mgr_->commit(context->transaction(), context->log_manager());
                context->set_transaction(nullptr);
                *txn_id = INVALID_TXN_ID;
                break;
            }
            case T_Transaction_rollback:
            case T_Transaction_abort: {
                if (context->transaction() == nullptr) {
                    throw NotImplementedError("Lab 4 transaction control");
                }
                context->set_transaction(txn_mgr_->get_transaction(*txn_id));
                txn_mgr_->abort(context->transaction(), context->log_manager());
                context->set_transaction(nullptr);
                *txn_id = INVALID_TXN_ID;
                break;
            }
            default:
                throw InternalError("Unexpected field type");
                break;
        }
    }
}

// 执行 select 语句，结构化结果通过当前请求的 Wire 响应返回。
void QlManager::select_from(std::unique_ptr<AbstractExecutor> executor_tree_root,
                            const std::vector<TabCol>& sel_cols,
                            Context* context) {
    std::vector<std::string> captions;
    captions.reserve(sel_cols.size());
    for (const auto& sel_col : sel_cols) {
        captions.push_back(sel_col.col_name);
    }

    const auto& proj_cols = executor_tree_root->cols();
    std::vector<WireResultColumn> columns;
    columns.reserve(proj_cols.size());
    for (size_t i = 0; i < proj_cols.size(); ++i) {
        columns.push_back({.name = i < captions.size() ? captions[i] : proj_cols[i].name, .type = proj_cols[i].type});
    }
    context->result().set_columns(std::move(columns));

    for (executor_tree_root->begin_tuple(); !executor_tree_root->is_end(); executor_tree_root->next_tuple()) {
        auto tuple = executor_tree_root->next();
        std::vector<WireResultCell> wire_row;
        wire_row.reserve(proj_cols.size());
        for (auto& col : proj_cols) {
            char* rec_buf = tuple->data + col.offset;
            WireResultCell cell{};
            cell.type = col.type;
            if (col.type == TYPE_INT) {
                std::memcpy(&cell.int_val, rec_buf, sizeof(cell.int_val));
            } else if (col.type == TYPE_FLOAT) {
                std::memcpy(&cell.float_val, rec_buf, sizeof(cell.float_val));
            } else if (col.type == TYPE_STRING) {
                const auto field = std::span(rec_buf, static_cast<size_t>(col.len));
                const auto string_end = std::ranges::find(field, '\0');
                cell.str_val.assign(field.begin(), string_end);
            }
            wire_row.push_back(std::move(cell));
        }
        context->result().add_row(std::move(wire_row));
    }
}

// 执行DML语句
void QlManager::run_dml(std::unique_ptr<AbstractExecutor> exec) { exec->next(); }
