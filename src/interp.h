#pragma once

#include <map>

#include "errors.h"
#include "execution/execution.h"
#include "parser/parser.h"
#include "system/sm.h"
#include "common/context.h"
#include "transaction/transaction_manager.h"

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

class Interp {
   private:
    SmManager *sm_manager_;
    QlManager *ql_manager_;
    TransactionManager *txn_mgr_;

   public:
    Interp(SmManager *sm_manager, QlManager *ql_manager, TransactionManager *txn_mgr) 
        : sm_manager_(sm_manager), ql_manager_(ql_manager), txn_mgr_(txn_mgr) {}

    void SetTransaction(txn_id_t *txn_id, Context *context) {
        context->txn_ = txn_mgr_->GetTransaction(*txn_id);
            if(context->txn_ == nullptr || context->txn_->GetState() == TransactionState::COMMITTED ||
                context->txn_->GetState() == TransactionState::ABORTED) {
                context->txn_ = txn_mgr_->Begin(nullptr, context->log_mgr_);
                *txn_id = context->txn_->GetTransactionId();
                context->txn_->SetTxnMode(false);
            }
    }

    void interp_sql(const std::shared_ptr<ast::TreeNode> &root, txn_id_t *txn_id, Context *context) {
        if (auto x = std::dynamic_pointer_cast<ast::Help>(root)) {
            // help;
            memcpy(context->data_send_ + *(context->offset_), help_info, strlen(help_info));
            *(context->offset_) = strlen(help_info);
        } else if (auto x = std::dynamic_pointer_cast<ast::ShowTables>(root)) {
            // show tables;
            SetTransaction(txn_id, context);
            sm_manager_->show_tables(context);
            if(context->txn_->GetTxnMode() == false)
                txn_mgr_->Commit(context->txn_, context->log_mgr_);
        } else if (auto x = std::dynamic_pointer_cast<ast::DescTable>(root)) {
            // desc table;
            SetTransaction(txn_id, context);
            sm_manager_->desc_table(x->tab_name, context);
            if(context->txn_->GetTxnMode() == false)
                txn_mgr_->Commit(context->txn_, context->log_mgr_);
        } else if (auto x = std::dynamic_pointer_cast<ast::CreateTable>(root)) {
            // create table;
            std::vector<ColDef> col_defs;
            for (auto &field : x->fields) {
                if (auto sv_col_def = std::dynamic_pointer_cast<ast::ColDef>(field)) {
                    ColDef col_def = {.name = sv_col_def->col_name,
                                      .type = interp_sv_type(sv_col_def->type_len->type),
                                      .len = sv_col_def->type_len->len};
                    col_defs.push_back(col_def);
                } else {
                    throw InternalError("Unexpected field type");
                }
            }
            SetTransaction(txn_id, context);
            sm_manager_->create_table(x->tab_name, col_defs, context);
            if(context->txn_->GetTxnMode() == false)
                txn_mgr_->Commit(context->txn_, context->log_mgr_);
        } else if (auto x = std::dynamic_pointer_cast<ast::DropTable>(root)) {
            // drop table;
            SetTransaction(txn_id, context);
            sm_manager_->drop_table(x->tab_name, context);
            if(context->txn_->GetTxnMode() == false)
                txn_mgr_->Commit(context->txn_, context->log_mgr_);
        } else if (auto x = std::dynamic_pointer_cast<ast::CreateIndex>(root)) {
            // create index;
            SetTransaction(txn_id, context);
            sm_manager_->create_index(x->tab_name, x->col_name, context);
            if(context->txn_->GetTxnMode() == false)
                txn_mgr_->Commit(context->txn_, context->log_mgr_);
        } else if (auto x = std::dynamic_pointer_cast<ast::DropIndex>(root)) {
            // drop index
            SetTransaction(txn_id, context);
            sm_manager_->drop_index(x->tab_name, x->col_name, context);
            if(context->txn_->GetTxnMode() == false)
                txn_mgr_->Commit(context->txn_, context->log_mgr_);
        } else if (auto x = std::dynamic_pointer_cast<ast::InsertStmt>(root)) {
            // insert;
            std::vector<Value> values;
            for (auto &sv_val : x->vals) {
                values.push_back(interp_sv_value(sv_val));
            }
            SetTransaction(txn_id, context);
            ql_manager_->insert_into(x->tab_name, values, context);
            if(context->txn_->GetTxnMode() == false)
                txn_mgr_->Commit(context->txn_, context->log_mgr_);
        } else if (auto x = std::dynamic_pointer_cast<ast::DeleteStmt>(root)) {
            // delete;
            std::vector<Condition> conds = interp_where_clause(x->conds);
            SetTransaction(txn_id, context);
            ql_manager_->delete_from(x->tab_name, conds, context);
            if(context->txn_->GetTxnMode() == false)
                txn_mgr_->Commit(context->txn_, context->log_mgr_);
        } else if (auto x = std::dynamic_pointer_cast<ast::UpdateStmt>(root)) {
            // update;
            std::vector<Condition> conds = interp_where_clause(x->conds);
            std::vector<SetClause> set_clauses;
            for (auto &sv_set_clause : x->set_clauses) {
                SetClause set_clause = {.lhs = {.tab_name = "", .col_name = sv_set_clause->col_name},
                                        .rhs = interp_sv_value(sv_set_clause->val)};
                set_clauses.push_back(set_clause);
            }
            SetTransaction(txn_id, context);
            ql_manager_->update_set(x->tab_name, set_clauses, conds, context);
            if(context->txn_->GetTxnMode() == false)
                txn_mgr_->Commit(context->txn_, context->log_mgr_);
        } else if (auto x = std::dynamic_pointer_cast<ast::SelectStmt>(root)) {
            // select;
            std::vector<Condition> conds = interp_where_clause(x->conds);
            std::vector<TabCol> sel_cols;
            for (auto &sv_sel_col : x->cols) {
                TabCol sel_col = {.tab_name = sv_sel_col->tab_name, .col_name = sv_sel_col->col_name};
                sel_cols.push_back(sel_col);
            }
            SetTransaction(txn_id, context);
            ql_manager_->select_from(sel_cols, x->tabs, conds, context);
            if(context->txn_->GetTxnMode() == false)
                txn_mgr_->Commit(context->txn_, context->log_mgr_);
        } else if (auto x = std::dynamic_pointer_cast<ast::TxnBegin>(root)) {
            // begin;
            context->txn_ = txn_mgr_->Begin(nullptr, context->log_mgr_);

            *txn_id = context->txn_->GetTransactionId();
            context->txn_->SetTxnMode(true);
        } else if (auto x = std::dynamic_pointer_cast<ast::TxnAbort>(root)) {
            // abort;
            context->txn_ = txn_mgr_->GetTransaction(*txn_id);
            txn_mgr_->Abort(context->txn_, context->log_mgr_);
        } else if (auto x = std::dynamic_pointer_cast<ast::TxnCommit>(root)) {
            // commit;
            context->txn_ = txn_mgr_->GetTransaction(*txn_id);
            txn_mgr_->Commit(context->txn_, context->log_mgr_);
        } else if (auto x = std::dynamic_pointer_cast<ast::TxnRollback>(root)) {
            // rollback;
            context->txn_ = txn_mgr_->GetTransaction(*txn_id);
            txn_mgr_->Abort(context->txn_, context->log_mgr_);
        } else {
            throw InternalError("Unexpected AST root");
        }
    }

   private:
    ColType interp_sv_type(ast::SvType sv_type) {
        std::map<ast::SvType, ColType> m = {
            {ast::SV_TYPE_INT, TYPE_INT}, {ast::SV_TYPE_FLOAT, TYPE_FLOAT}, {ast::SV_TYPE_STRING, TYPE_STRING}};
        return m.at(sv_type);
    }

    CompOp interp_sv_comp_op(ast::SvCompOp op) {
        std::map<ast::SvCompOp, CompOp> m = {
            {ast::SV_OP_EQ, OP_EQ}, {ast::SV_OP_NE, OP_NE}, {ast::SV_OP_LT, OP_LT},
            {ast::SV_OP_GT, OP_GT}, {ast::SV_OP_LE, OP_LE}, {ast::SV_OP_GE, OP_GE},
        };
        return m.at(op);
    }

    Value interp_sv_value(const std::shared_ptr<ast::Value> &sv_val) {
        Value val;
        if (auto int_lit = std::dynamic_pointer_cast<ast::IntLit>(sv_val)) {
            val.set_int(int_lit->val);
        } else if (auto float_lit = std::dynamic_pointer_cast<ast::FloatLit>(sv_val)) {
            val.set_float(float_lit->val);
        } else if (auto str_lit = std::dynamic_pointer_cast<ast::StringLit>(sv_val)) {
            val.set_str(str_lit->val);
        } else {
            throw InternalError("Unexpected sv value type");
        }
        return val;
    }

    std::vector<Condition> interp_where_clause(const std::vector<std::shared_ptr<ast::BinaryExpr>> &sv_conds) {
        std::vector<Condition> conds;
        for (auto &expr : sv_conds) {
            Condition cond;
            cond.lhs_col = {.tab_name = expr->lhs->tab_name, .col_name = expr->lhs->col_name};
            cond.op = interp_sv_comp_op(expr->op);
            if (auto rhs_val = std::dynamic_pointer_cast<ast::Value>(expr->rhs)) {
                cond.is_rhs_val = true;
                cond.rhs_val = interp_sv_value(rhs_val);
            } else if (auto rhs_col = std::dynamic_pointer_cast<ast::Col>(expr->rhs)) {
                cond.is_rhs_val = false;
                cond.rhs_col = {.tab_name = rhs_col->tab_name, .col_name = rhs_col->col_name};
            }
            conds.push_back(cond);
        }
        return conds;
    }
};
