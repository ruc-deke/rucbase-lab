// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "execution_manager.h"

#include <algorithm>

#include "executor_delete.h"
#include "executor_index_scan.h"
#include "executor_insert.h"
#include "executor_nestedloop_join.h"
#include "executor_projection.h"
#include "executor_seq_scan.h"
#include "executor_update.h"
#include "index/ix.h"

const char *help_info = "Supported SQL syntax:\n"
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

// 主要负责执行DDL语句
void QlManager::run_mutli_query(std::shared_ptr<Plan> plan, Context *context){
    if (auto x = std::dynamic_pointer_cast<DDLPlan>(plan)) {
        switch(x->tag) {
            case T_CreateTable:
            {
                sm_manager_->create_table(x->tab_name_, x->cols_, context);
                break;
            }
            case T_DropTable:
            {
                sm_manager_->drop_table(x->tab_name_, context);
                break;
            }
            case T_CreateIndex:
            {
                sm_manager_->create_index(x->tab_name_, x->tab_col_names_, context);
                break;
            }
            case T_DropIndex:
            {
                sm_manager_->drop_index(x->tab_name_, x->tab_col_names_, context);
                break;
            }
            default:
                throw InternalError("Unexpected field type");
                break;  
        }
    }
}

// 执行help; show tables; desc table; begin; commit; abort;语句
void QlManager::run_cmd_utility(std::shared_ptr<Plan> plan, txn_id_t *txn_id, Context *context) {
    if (auto x = std::dynamic_pointer_cast<OtherPlan>(plan)) {
        switch(x->tag) {
            case T_Help:
            {
                memcpy(context->data_send_ + *(context->offset_), help_info, strlen(help_info));
                *(context->offset_) = strlen(help_info);
                break;
            }
            case T_ShowTable:
            {
                sm_manager_->show_tables(context);
                break;
            }
            case T_DescTable:
            {
                sm_manager_->desc_table(x->tab_name_, context);
                break;
            }
            case T_Transaction_begin:
            {
                // 显示开启一个事务
                context->txn_->set_txn_mode(true);
                break;
            }  
            case T_Transaction_commit:
            {
                context->txn_ = txn_mgr_->get_transaction(*txn_id);
                txn_mgr_->commit(context->txn_, context->log_mgr_);
                break;
            }    
            case T_Transaction_rollback:
            {
                context->txn_ = txn_mgr_->get_transaction(*txn_id);
                txn_mgr_->abort(context->txn_, context->log_mgr_);
                break;
            }    
            case T_Transaction_abort:
            {
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
void QlManager::select_from(std::unique_ptr<AbstractExecutor> executorTreeRoot, std::vector<TabCol> sel_cols, 
                            Context *context) {
    std::vector<std::string> captions;
    captions.reserve(sel_cols.size());
    for (auto &sel_col : sel_cols) {
        captions.push_back(sel_col.col_name);
    }

    // Build typed wire result (META/ROW) from executor schema + tuples.
    // Wire clients format the typed result; data_send is not used for SELECT.
    context->wire_result_ = WireResultSet{};
    context->wire_result_.has_query_result = true;
    const auto &proj_cols = executorTreeRoot->cols();
    context->wire_result_.columns.reserve(proj_cols.size());
    for (size_t i = 0; i < proj_cols.size(); ++i) {
        WireResultColumn column;
        column.name = i < captions.size() ? captions[i] : proj_cols[i].name;
        column.type = proj_cols[i].type;
        context->wire_result_.columns.push_back(std::move(column));
    }
    size_t schema_bytes = context->wire_result_.columns.size() * sizeof(WireResultColumn);
    if (schema_bytes > WireResultSet::kMaxBufferedBytes) {
        throw InternalError("query result schema exceeds the 16 MiB teaching wire buffer");
    }
    for (const auto &column : context->wire_result_.columns) {
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
        size_t wire_row_bytes = 2 * sizeof(std::vector<WireResultCell>) +
                                proj_cols.size() * sizeof(WireResultCell);
        if (wire_row_bytes > WireResultSet::kMaxBufferedBytes) {
            throw InternalError("query result exceeds the 16 MiB teaching wire buffer");
        }
        wire_row.reserve(proj_cols.size());
        for (auto &col : proj_cols) {
            char *rec_buf = Tuple->data + col.offset;
            WireResultCell cell;
            cell.type = col.type;
            if (col.type == TYPE_INT) {
                cell.int_val = *reinterpret_cast<int *>(rec_buf);
            } else if (col.type == TYPE_FLOAT) {
                cell.float_val = *reinterpret_cast<float *>(rec_buf);
            } else if (col.type == TYPE_STRING) {
                const char *string_end = std::find(rec_buf, rec_buf + col.len, '\0');
                cell.str_val.assign(rec_buf, static_cast<size_t>(string_end - rec_buf));
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
void QlManager::run_dml(std::unique_ptr<AbstractExecutor> exec){
    exec->Next();
}
