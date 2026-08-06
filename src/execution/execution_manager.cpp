/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "execution_manager.h"

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

    // Execute the query plan and collect the bounded typed result.
    for (executorTreeRoot->beginTuple(); !executorTreeRoot->is_end(); executorTreeRoot->nextTuple()) {
        auto Tuple = executorTreeRoot->Next();
        std::vector<WireResultCell> wire_row;
        size_t wire_row_bytes = 0;
        wire_row.reserve(proj_cols.size());
        for (auto &col : proj_cols) {
            std::string col_str;
            char *rec_buf = Tuple->data + col.offset;
            WireResultCell cell;
            cell.type = col.type;
            if (col.type == TYPE_INT) {
                cell.int_val = *reinterpret_cast<int *>(rec_buf);
                col_str = std::to_string(cell.int_val);
                wire_row_bytes += 1 + sizeof(int32_t);
            } else if (col.type == TYPE_FLOAT) {
                cell.float_val = *reinterpret_cast<float *>(rec_buf);
                col_str = std::to_string(cell.float_val);
                wire_row_bytes += 1 + sizeof(float);
            } else if (col.type == TYPE_STRING) {
                col_str = std::string(rec_buf, col.len);
                col_str.resize(strlen(col_str.c_str()));
                cell.str_val = col_str;
                wire_row_bytes += 1 + sizeof(uint32_t) + cell.str_val.size();
            }
            wire_row.push_back(std::move(cell));
        }
        if (context->wire_result_.buffered_bytes > WireResultSet::kMaxBufferedBytes ||
            wire_row_bytes > WireResultSet::kMaxBufferedBytes - context->wire_result_.buffered_bytes) {
            throw InternalError("query result exceeds the 16 MiB teaching wire buffer");
        }
        context->wire_result_.buffered_bytes += wire_row_bytes;
        context->wire_result_.rows.push_back(std::move(wire_row));
    }
}

// 执行DML语句
void QlManager::run_dml(std::unique_ptr<AbstractExecutor> exec){
    exec->Next();
}
