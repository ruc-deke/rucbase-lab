// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common/common.h"
#include "common/config.h"
#include "common/context.h"
#include "common/defs.h"
#include "common/errors.h"
#include "executor_abstract.h"
#include "index/b_plus_tree.h"
#include "index/index_manager.h"
#include "record/rm_defs.h"
#include "record/rm_file_handle.h"
#include "system/sm_manager.h"
#include "system/sm_meta.h"

class InsertExecutor : public AbstractExecutor {
private:
    TabMeta tab_;                                         // 表的元数据
    std::vector<Value> values_;                           // 需要插入的数据
    RmFileHandle* fh_;                                    // 表的数据文件句柄
    std::string tab_name_;                                // 表名称
    Rid rid_{.page_no = INVALID_PAGE_ID, .slot_no = -1};  // 成功插入后替换为实际位置。
    SmManager* sm_manager_;

public:
    InsertExecutor(SmManager* sm_manager, const std::string& tab_name, std::vector<Value> values, Context* context) {
        sm_manager_ = sm_manager;
        tab_ = sm_manager_->db_.get_table(tab_name);
        tab_name_ = tab_name;
        if (values.size() != tab_.cols.size()) {
            throw InvalidValueCountError();
        }
        // 校验通过后再 move，避免异常路径上留下半初始化状态。
        values_ = std::move(values);
        fh_ = sm_manager_->fhs_.at(tab_name).get();
        context_ = context;
    };

    std::unique_ptr<RmRecord> next() override {
        // Make record buffer
        RmRecord rec(fh_->get_file_hdr().record_size);
        for (size_t i = 0; i < values_.size(); i++) {
            auto& col = tab_.cols[i];
            auto& val = values_[i];
            if (col.type != val.type) {
                throw IncompatibleTypeError(coltype2str(col.type), coltype2str(val.type));
            }
            if (val.raw == nullptr) {
                // Keep direct executor construction safe while allowing Analyzer to pre-encode values.
                val.init_raw(col.len);
            } else if (val.raw->size != col.len) {
                throw InternalError("Insert value raw size does not match column length");
            }
            memcpy(rec.data + col.offset, val.raw->data, col.len);
        }

        rid_ = fh_->insert_record(rec.data, context_);

        // 逐个维护索引。唯一索引可能抛出 DuplicateKeyError，此时要撤销本条记录已经写入的
        // 全部内容：先删除前面索引中已插入的 (key, rid)，再删除记录，保证表和索引保持一致。
        std::vector<std::pair<BPlusTree*, std::vector<char>>> inserted;
        try {
            for (auto& index : tab_.indexes) {
                auto ih =
                    sm_manager_->indexes_.at(sm_manager_->get_index_manager()->make_index_name(tab_name_, index.cols))
                        .get();
                std::vector<char> key = index.make_key(rec.data);
                ih->insert_entry(key.data(), rid_, context_->transaction());
                inserted.emplace_back(ih, std::move(key));
            }
        } catch (...) {
            for (auto it = inserted.rbegin(); it != inserted.rend(); ++it) {
                it->first->delete_entry(it->second.data(), rid_, context_->transaction());
            }
            fh_->delete_record(rid_, context_);
            throw;
        }
        return nullptr;
    }
    Rid& rid() override { return rid_; }
};
