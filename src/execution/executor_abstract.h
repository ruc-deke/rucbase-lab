// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "common/common.h"
#include "common/defs.h"
#include "common/errors.h"
#include "record/rm_defs.h"
#include "system/sm_meta.h"

class Context;

class AbstractExecutor {
   public:
    Rid _abstract_rid{};

    Context *context_ = nullptr;

    virtual ~AbstractExecutor() = default;

    virtual size_t tupleLen() const { throw NotImplementedError("AbstractExecutor::tupleLen"); };

    virtual const std::vector<ColMeta> &cols() const {
        throw NotImplementedError("AbstractExecutor::cols");
    };

    virtual std::string getType() { return "AbstractExecutor"; };

    virtual void beginTuple() { throw NotImplementedError("AbstractExecutor::beginTuple"); };

    virtual void nextTuple() { throw NotImplementedError("AbstractExecutor::nextTuple"); };

    virtual bool is_end() const { throw NotImplementedError("AbstractExecutor::is_end"); };

    virtual Rid &rid() = 0;

    virtual std::unique_ptr<RmRecord> Next() = 0;

    virtual ColMeta get_col_offset(const TabCol &target) {
        throw NotImplementedError("AbstractExecutor::get_col_offset");
    };

    /**
     * @brief 在记录 schema 中查找目标列。
     * @note C++20：`std::ranges::find_if` 对整个 vector 查找，返回的 const_iterator 与
     *       旧式 `find_if(begin, end, pred)` 相同，学生作业里也可继续用后者。
     */
    static std::vector<ColMeta>::const_iterator get_col(const std::vector<ColMeta> &rec_cols,
                                                        const TabCol &target) {
        const auto pos = std::ranges::find_if(rec_cols, [&](const ColMeta &col) {
            return col.tab_name == target.tab_name && col.name == target.col_name;
        });
        if (pos == rec_cols.end()) {
            throw ColumnNotFoundError(target.tab_name + '.' + target.col_name);
        }
        return pos;
    }
};
