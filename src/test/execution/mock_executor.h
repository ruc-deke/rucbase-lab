// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

/**
 * @file mock_executor.h
 * @brief 执行器单元测试使用的内存数据源和记录编解码工具。
 */

#pragma once

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "execution/executor_abstract.h"

namespace rucbase::test {

/** @brief 一个字段的取值。 */
using Cell = std::variant<int, float, std::string>;
/** @brief 一条记录的全部字段取值。 */
using Row = std::vector<Cell>;

/** @brief 按声明顺序连续排列字段，生成表的字段元数据。 */
inline std::vector<ColMeta> make_schema(const std::string& tab_name,
                                        const std::vector<std::pair<std::string, std::pair<ColType, int>>>& fields) {
    std::vector<ColMeta> cols;
    int offset = 0;
    for (const auto& [name, type_len] : fields) {
        cols.push_back({.tab_name = tab_name,
                        .name = name,
                        .type = type_len.first,
                        .len = type_len.second,
                        .offset = offset,
                        .index = false});
        offset += type_len.second;
    }
    return cols;
}

inline size_t schema_len(const std::vector<ColMeta>& cols) {
    return cols.empty() ? 0 : static_cast<size_t>(cols.back().offset + cols.back().len);
}

/** @brief 按字段元数据把一行取值编码为定长记录。 */
inline std::vector<char> encode_row(const std::vector<ColMeta>& cols, const Row& row) {
    std::vector<char> data(schema_len(cols), 0);
    for (size_t i = 0; i < cols.size(); ++i) {
        const ColMeta& col = cols[i];
        char* dest = data.data() + col.offset;
        switch (col.type) {
            case TYPE_INT: {
                const int value = std::get<int>(row.at(i));
                memcpy(dest, &value, sizeof(value));
                break;
            }
            case TYPE_FLOAT: {
                const float value = std::get<float>(row.at(i));
                memcpy(dest, &value, sizeof(value));
                break;
            }
            case TYPE_STRING: {
                const std::string& value = std::get<std::string>(row.at(i));
                // CHAR(n) 是定长字段，不足部分保持为 0，不需要额外的结尾 '\0'。
                std::copy_n(value.data(), std::min(value.size(), static_cast<size_t>(col.len)), dest);
                break;
            }
        }
    }
    return data;
}

/** @brief 按字段元数据从记录中解码一行取值。 */
inline Row decode_row(const std::vector<ColMeta>& cols, const char* data) {
    Row row;
    row.reserve(cols.size());
    for (const ColMeta& col : cols) {
        const char* src = data + col.offset;
        switch (col.type) {
            case TYPE_INT: {
                int value;
                memcpy(&value, src, sizeof(value));
                row.emplace_back(value);
                break;
            }
            case TYPE_FLOAT: {
                float value;
                memcpy(&value, src, sizeof(value));
                row.emplace_back(value);
                break;
            }
            case TYPE_STRING:
                row.emplace_back(std::string(src, strnlen(src, static_cast<size_t>(col.len))));
                break;
        }
    }
    return row;
}

/**
 * @brief 按给定顺序输出内存中若干记录的叶子算子。
 *
 * 记录 begin_tuple() 的调用次数，用于检查上层算子是否重复扫描孩子。
 */
class MockExecutor : public AbstractExecutor {
public:
    MockExecutor(std::vector<ColMeta> cols, const std::vector<Row>& rows) : cols_(std::move(cols)) {
        len_ = schema_len(cols_);
        for (const Row& row : rows) {
            records_.push_back(encode_row(cols_, row));
        }
    }

    size_t tuple_len() const override { return len_; }

    const std::vector<ColMeta>& cols() const override { return cols_; }

    std::string executor_type() override { return "MockExecutor"; }

    void begin_tuple() override {
        ++begin_calls_;
        position_ = 0;
    }

    void next_tuple() override {
        if (position_ < records_.size()) {
            ++position_;
        }
    }

    bool is_end() const override { return position_ >= records_.size(); }

    std::unique_ptr<RmRecord> next() override {
        if (is_end()) {
            throw InternalError("MockExecutor::next called after the end");
        }
        return std::make_unique<RmRecord>(static_cast<int>(len_), records_[position_].data());
    }

    Rid& rid() override {
        rid_ = {.page_no = 1, .slot_no = static_cast<int>(position_)};
        return rid_;
    }

    ColMeta get_col_offset(const TabCol& target) override { return *get_col(cols_, target); }

    [[nodiscard]] int begin_calls() const noexcept { return begin_calls_; }

private:
    std::vector<ColMeta> cols_;
    size_t len_ = 0;
    std::vector<std::vector<char>> records_;
    size_t position_ = 0;
    int begin_calls_ = 0;
    Rid rid_{};
};

/** @brief 通过 AbstractExecutor 的迭代接口读出全部输出记录。 */
inline std::vector<Row> drain(AbstractExecutor& executor) {
    std::vector<Row> rows;
    const std::vector<ColMeta> cols = executor.cols();
    for (executor.begin_tuple(); !executor.is_end(); executor.next_tuple()) {
        auto record = executor.next();
        if (record == nullptr) {
            throw InternalError("executor returned a null record before reaching the end");
        }
        rows.push_back(decode_row(cols, record->data));
    }
    return rows;
}

}  // namespace rucbase::test
