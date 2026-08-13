// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <algorithm>
#include <cstring>
#include <iosfwd>
#include <map>
#include <string>
#include <vector>

#include "common/defs.h"
#include "common/errors.h"

/** @brief 字段的目录元数据，包含其定长记录布局。 */
struct ColMeta {
    std::string tab_name;  ///< 所属表，由 JSON 的父级表名推导。
    std::string name;
    ColType type{};
    int len{};
    int offset{};       ///< 由字段声明顺序和长度推导。
    bool index{false};  ///< 是否参与任一索引，由 indexes 推导。
};

/**
 * @brief 一条索引的定义。
 *
 * 索引按 (key, Rid) 存放。unique 为 false 时同一 key 可以对应多条记录；
 * 为 true 时每个 key 至多一条。cols 的顺序就是复合键顺序。
 */
struct IndexMeta {
    std::string tab_name;
    int col_tot_len{};          ///< 由 cols 推导。
    int col_num{};              ///< 由 cols.size() 推导。
    bool unique{false};         ///< 是否施加唯一约束。
    std::vector<ColMeta> cols;  ///< 索引包含的字段。

    static IndexMeta make(std::string table_name, std::vector<ColMeta> columns, bool unique = false) {
        IndexMeta index;
        index.tab_name = std::move(table_name);
        index.cols = std::move(columns);
        index.unique = unique;
        index.rebuild_derived();
        return index;
    }

    void rebuild_derived() {
        col_num = static_cast<int>(cols.size());
        col_tot_len = 0;
        for (const auto& column : cols) {
            col_tot_len += column.len;
        }
    }

    void encode_key(const char* record, char* key_out) const {
        int offset = 0;
        for (const auto& column : cols) {
            std::memcpy(key_out + offset, record + column.offset, static_cast<size_t>(column.len));
            offset += column.len;
        }
    }

    [[nodiscard]] std::vector<char> make_key(const char* record) const {
        std::vector<char> key(static_cast<size_t>(col_tot_len));
        encode_key(record, key.data());
        return key;
    }
};

/** @brief 表的目录元数据；字段和索引均保留声明顺序。 */
struct TabMeta {
    std::string name;
    std::vector<ColMeta> cols;
    std::vector<IndexMeta> indexes;

    /**
     * @brief 判断表中是否存在指定字段。
     * @note C++20：`std::ranges::find_if(range, pred)` 等价于
     *       `std::find_if(range.begin(), range.end(), pred)`，无需手写迭代器对。
     */
    bool is_col(const std::string& col_name) const {
        return std::ranges::find_if(cols, [&](const ColMeta& col) { return col.name == col_name; }) != cols.end();
    }

    /**
     * @brief 判断索引的字段及顺序是否与给定字段名完全一致。
     * @note 复合索引的字段顺序属于索引定义的一部分。
     * @note C++20：`std::ranges::equal` 可直接比较两个 range，并可带二元谓词。
     */
    static bool matches_index(const IndexMeta& index, const std::vector<std::string>& col_names) {
        return index.cols.size() == col_names.size() &&
               std::ranges::equal(index.cols, col_names,
                                  [](const ColMeta& col, const std::string& name) { return col.name == name; });
    }

    /**
     * @brief 判断表中是否存在字段及顺序完全匹配的索引。
     * @note C++20：`std::ranges::any_of` 表示「是否存在任一元素满足谓词」。
     */
    bool is_index(const std::vector<std::string>& col_names) const {
        return std::ranges::any_of(indexes, [&](const IndexMeta& index) { return matches_index(index, col_names); });
    }

    /**
     * @brief 查找可修改的索引元数据。
     * @throws IndexNotFoundError 未找到字段及顺序完全匹配的索引。
     */
    std::vector<IndexMeta>::iterator get_index_meta(const std::vector<std::string>& col_names) {
        // ranges::find_if 返回的迭代器类型与 vector::iterator 兼容，可继续用于 erase 等接口。
        const auto pos =
            std::ranges::find_if(indexes, [&](const IndexMeta& index) { return matches_index(index, col_names); });
        if (pos == indexes.end()) throw IndexNotFoundError(name, col_names);
        return pos;
    }

    /**
     * @brief 查找只读索引元数据。
     * @throws IndexNotFoundError 未找到字段及顺序完全匹配的索引。
     */
    std::vector<IndexMeta>::const_iterator get_index_meta(const std::vector<std::string>& col_names) const {
        const auto pos =
            std::ranges::find_if(indexes, [&](const IndexMeta& index) { return matches_index(index, col_names); });
        if (pos == indexes.end()) throw IndexNotFoundError(name, col_names);
        return pos;
    }

    /** @brief 查找可修改的字段元数据。 @throws ColumnNotFoundError 字段不存在。 */
    std::vector<ColMeta>::iterator get_col(const std::string& col_name) {
        const auto pos = std::ranges::find_if(cols, [&](const ColMeta& col) { return col.name == col_name; });
        if (pos == cols.end()) {
            throw ColumnNotFoundError(col_name);
        }
        return pos;
    }

    /** @brief 查找只读字段元数据。 @throws ColumnNotFoundError 字段不存在。 */
    std::vector<ColMeta>::const_iterator get_col(const std::string& col_name) const {
        const auto pos = std::ranges::find_if(cols, [&](const ColMeta& col) { return col.name == col_name; });
        if (pos == cols.end()) throw ColumnNotFoundError(col_name);
        return pos;
    }
};

/** @brief 当前数据库的内存目录，是 db.meta JSON 的运行时表示。 */
class DbMeta {
    friend class SmManager;

private:
    std::string name_;
    std::map<std::string, TabMeta> tabs_;  // map 让 JSON 输出顺序稳定

public:
    /**
     * @brief 判断目录中是否存在指定表。
     * @note C++20：`map::contains(k)` 等价于 `find(k) != end()`，语义更直观。
     */
    bool is_table(const std::string& tab_name) const { return tabs_.contains(tab_name); }

    /**
     * @brief 新增或替换表元数据。
     * @throws InternalError map 键与表元数据中的名称不一致。
     */
    void SetTabMeta(const std::string& tab_name, const TabMeta& meta) {
        if (tab_name != meta.name) throw InternalError("Table metadata name mismatch: " + tab_name);
        tabs_[tab_name] = meta;
    }

    /** @brief 查找可修改的表元数据。 @throws TableNotFoundError 表不存在。 */
    TabMeta& get_table(const std::string& tab_name) {
        const auto pos = tabs_.find(tab_name);
        if (pos == tabs_.end()) {
            throw TableNotFoundError(tab_name);
        }
        return pos->second;
    }

    /** @brief 查找只读表元数据。 @throws TableNotFoundError 表不存在。 */
    const TabMeta& get_table(const std::string& tab_name) const {
        const auto pos = tabs_.find(tab_name);
        if (pos == tabs_.end()) throw TableNotFoundError(tab_name);
        return pos->second;
    }

    /** @brief 将数据库目录写为两空格缩进的 JSON。 */
    friend std::ostream& operator<<(std::ostream& os, const DbMeta& db_meta);

    /**
     * @brief 从 JSON 重建数据库目录。
     * @post 仅在完整解析和校验成功后替换 db_meta。
     */
    friend std::istream& operator>>(std::istream& is, DbMeta& db_meta);
};
