// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "execution_manager.h"

#include <algorithm>
#include <span>

#include "executor_delete.h"
#include "executor_index_scan.h"
#include "executor_insert.h"
#include "executor_nestedloop_join.h"
#include "executor_projection.h"
#include "executor_seq_scan.h"
#include "executor_update.h"
#include "index/ix.h"

constexpr char help_info[] =
    "Supported SQL syntax:\n"
    "  command ;\n"
    "command:\n"
    "  CREATE TABLE table_name (column_name type [, column_name type ...])\n"
    "  DROP TABLE table_name\n"
    "  CREATE INDEX table_name (column_name)\n"
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

constexpr int help_info_size = static_cast<int>(sizeof(help_info) - 1);

// 主要负责执行DDL语句
void QlManager::run_mutli_query(const std::shared_ptr<Plan>& plan, Context* context) const {
    if (const auto ddl_plan = std::dynamic_pointer_cast<DDLPlan>(plan)) {
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
                sm_manager_->create_index(ddl_plan->tab_name_, ddl_plan->tab_col_names_, context);
                break;
            }
            case T_DropIndex: {
                sm_manager_->drop_index(ddl_plan->tab_name_, ddl_plan->tab_col_names_, context);
                break;
            }
            default:
                throw InternalError("Unexpected field type");
                break;
        }
    }
}

// 执行help; show tables; desc table; begin; commit; abort;语句
void QlManager::run_cmd_utility(const std::shared_ptr<Plan>& plan, txn_id_t* txn_id, Context* context) {
    if (const auto utility_plan = std::dynamic_pointer_cast<OtherPlan>(plan)) {
        switch (utility_plan->tag) {
            case T_Help: {
                memcpy(context->data_send_ + *(context->offset_), help_info, help_info_size);
                *(context->offset_) = help_info_size;
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
                if (context->txn_ == nullptr) {
                    throw NotImplementedError("Lab 4 transaction control");
                }
                // 显示开启一个事务
                context->txn_->set_txn_mode(true);
                break;
            }
            case T_Transaction_commit: {
                if (context->txn_ == nullptr) {
                    throw NotImplementedError("Lab 4 transaction control");
                }
                context->txn_ = txn_mgr_->get_transaction(*txn_id);
                txn_mgr_->commit(context->txn_, context->log_mgr_);
                break;
            }
            case T_Transaction_rollback:
            case T_Transaction_abort: {
                if (context->txn_ == nullptr) {
                    throw NotImplementedError("Lab 4 transaction control");
                }
                context->txn_ = txn_mgr_->get_transaction(*txn_id);
                txn_mgr_->abort(context->txn_, context->log_mgr_);
                break;
            }
            default:
                throw InternalError("Unexpected field type");
                break;
        }
    }
}

// 执行 select 语句，结构化结果通过当前请求的 Wire 响应返回。
void QlManager::select_from(std::unique_ptr<AbstractExecutor> executorTreeRoot,
                            const std::vector<TabCol>& sel_cols,
                            Context* context) {
    std::vector<std::string> captions;
    captions.reserve(sel_cols.size());
    for (const auto& sel_col : sel_cols) {
        captions.push_back(sel_col.col_name);
    }

    // Build typed wire result (META/ROW) from executor schema + tuples.
    // Wire clients format the typed result; data_send is not used for SELECT.
    context->wire_result_ = WireResultSet{};
    context->wire_result_.has_query_result = true;
    const auto& proj_cols = executorTreeRoot->cols();
    context->wire_result_.columns.reserve(proj_cols.size());
    for (size_t i = 0; i < proj_cols.size(); ++i) {
        WireResultColumn column{.name = i < captions.size() ? captions[i] : proj_cols[i].name,
                                .type = proj_cols[i].type};
        context->wire_result_.columns.push_back(std::move(column));
    }
    size_t schema_bytes = context->wire_result_.columns.size() * sizeof(WireResultColumn);
    if (schema_bytes > WireResultSet::kMaxBufferedBytes) {
        throw InternalError("query result schema exceeds the 16 MiB teaching wire buffer");
    }
    for (const auto& column : context->wire_result_.columns) {
        if (column.name.size() > WireResultSet::kMaxBufferedBytes - schema_bytes) {
            throw InternalError("query result schema exceeds the 16 MiB teaching wire buffer");
        }
        schema_bytes += column.name.size();
    }
    if (!context->wire_result_.try_account(schema_bytes)) {
        throw InternalError("query result schema exceeds the 16 MiB teaching wire buffer");
    }

    // Execute the query plan and collect the bounded typed result.
    for (executorTreeRoot->beginTuple(); !executorTreeRoot->is_end(); executorTreeRoot->nextTuple()) {
        auto Tuple = executorTreeRoot->Next();
        std::vector<WireResultCell> wire_row;
        // Account for the retained row vector, its cells, and conservative
        // growth slack in the outer rows vector.
        size_t wire_row_bytes = 2 * sizeof(std::vector<WireResultCell>) + proj_cols.size() * sizeof(WireResultCell);
        if (wire_row_bytes > WireResultSet::kMaxBufferedBytes) {
            throw InternalError("query result exceeds the 16 MiB teaching wire buffer");
        }
        wire_row.reserve(proj_cols.size());
        for (auto& col : proj_cols) {
            char* rec_buf = Tuple->data + col.offset;
            WireResultCell cell{.type = col.type};
            if (col.type == TYPE_INT) {
                std::memcpy(&cell.int_val, rec_buf, sizeof(cell.int_val));
            } else if (col.type == TYPE_FLOAT) {
                std::memcpy(&cell.float_val, rec_buf, sizeof(cell.float_val));
            } else if (col.type == TYPE_STRING) {
                // C++20：std::span 是「不拥有内存」的连续视图，这里表示定长 CHAR 字段字节。
                // ranges::find 在视图内找 '\0'；等价于 std::find(rec_buf, rec_buf+len, '\0')。
                const auto field = std::span(rec_buf, static_cast<size_t>(col.len));
                const auto string_end = std::ranges::find(field, '\0');
                cell.str_val.assign(field.begin(), string_end);
                if (cell.str_val.size() > WireResultSet::kMaxBufferedBytes - wire_row_bytes) {
                    throw InternalError("query result exceeds the 16 MiB teaching wire buffer");
                }
                wire_row_bytes += cell.str_val.size();
            }
            wire_row.push_back(std::move(cell));
        }
        if (!context->wire_result_.try_account(wire_row_bytes)) {
            throw InternalError("query result exceeds the 16 MiB teaching wire buffer");
        }
        context->wire_result_.rows.push_back(std::move(wire_row));
    }
}

// 执行DML语句
void QlManager::run_dml(std::unique_ptr<AbstractExecutor> exec) { exec->Next(); }
