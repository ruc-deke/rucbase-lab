// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include "common/errors.h"
#include "execution_defs.h"
#include "common/common.h"
#include "index/ix.h"
#include "system/sm.h"

class AbstractExecutor {
   public:
    Rid _abstract_rid;

    Context *context_;

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

    std::vector<ColMeta>::const_iterator get_col(const std::vector<ColMeta> &rec_cols, const TabCol &target) {
        auto pos = std::find_if(rec_cols.begin(), rec_cols.end(), [&](const ColMeta &col) {
            return col.tab_name == target.tab_name && col.name == target.col_name;
        });
        if (pos == rec_cols.end()) {
            throw ColumnNotFoundError(target.tab_name + '.' + target.col_name);
        }
        return pos;
    }
};
