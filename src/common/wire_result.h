// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "common/defs.h"
#include "common/errors.h"

/**
 * @file wire_result.h
 * @brief 服务端查询结果缓冲。Lab 作业不必阅读或修改。
 */

struct WireResultColumn {
    std::string name;
    ColType type = TYPE_STRING;
};

struct WireResultCell {
    ColType type = TYPE_STRING;
    int int_val = 0;
    float float_val = 0.0f;
    std::string str_val;
};

struct WireResultSet {
    static constexpr size_t kMaxBufferedBytes = size_t{16} * 1024 * 1024;
    static constexpr size_t kMaxRawTextBytes = size_t{1} * 1024 * 1024;

    bool has_query_result = false;
    bool raw_text = false;
    size_t buffered_bytes = 0;
    std::vector<WireResultColumn> columns;
    std::vector<std::vector<WireResultCell>> rows;

    bool try_account(size_t bytes) noexcept {
        if (buffered_bytes > kMaxBufferedBytes || bytes > kMaxBufferedBytes - buffered_bytes) {
            return false;
        }
        buffered_bytes += bytes;
        return true;
    }
};

class QueryResultBuilder {
public:
    void reset_query() { result_ = WireResultSet{}; }

    void set_columns(std::vector<WireResultColumn> columns) {
        result_ = WireResultSet{};
        result_.has_query_result = true;
        size_t bytes = columns.size() * sizeof(WireResultColumn);
        for (const auto& column : columns) {
            bytes += column.name.size();
        }
        if (!result_.try_account(bytes)) {
            throw InternalError("query result schema exceeds the 16 MiB teaching wire buffer");
        }
        result_.columns = std::move(columns);
    }

    void set_raw_text(std::string column_name, std::string text) {
        if (text.size() > WireResultSet::kMaxRawTextBytes) {
            throw InternalError("raw text result exceeds the 1 MiB wire frame limit");
        }
        result_ = WireResultSet{};
        result_.has_query_result = true;
        result_.raw_text = true;
        const size_t bytes = sizeof(WireResultColumn) + column_name.size() + text.size();
        if (!result_.try_account(bytes)) {
            throw InternalError("raw text result exceeds the teaching wire buffer");
        }
        result_.columns.push_back({.name = std::move(column_name), .type = TYPE_STRING});
        result_.rows.push_back({{.type = TYPE_STRING, .str_val = std::move(text)}});
    }

    void add_row(std::vector<WireResultCell> row) {
        size_t bytes = 2 * sizeof(std::vector<WireResultCell>) + row.size() * sizeof(WireResultCell);
        for (const auto& cell : row) {
            bytes += cell.str_val.size();
        }
        if (!result_.try_account(bytes)) {
            throw InternalError("query result exceeds the 16 MiB teaching wire buffer");
        }
        result_.rows.push_back(std::move(row));
    }

    [[nodiscard]] const WireResultSet& view() const noexcept { return result_; }
    [[nodiscard]] WireResultSet take() noexcept { return std::move(result_); }

private:
    WireResultSet result_;
};
