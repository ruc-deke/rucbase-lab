// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "system/sm_meta.h"

#include <algorithm>
#include <istream>
#include <ostream>
#include <utility>

#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

#include "record/rm_defs.h"

namespace {

using Json = nlohmann::ordered_json;

/** @brief 统一处理 db.meta 的格式错误或目录不变量错误。 */
[[noreturn]] void invalid_meta() {
    throw InternalError("Invalid db.meta JSON; recreate databases written by older versions");
}

/** @brief 要求 JSON 值为对象。 */
void require_object(const Json& value) {
    if (!value.is_object()) {
        invalid_meta();
    }
}

/** @brief 读取并检查数组字段。 */
const Json& array_field(const Json& object, const char* key) {
    const Json& value = object.at(key);
    if (!value.is_array()) {
        invalid_meta();
    }
    return value;
}

/** @brief 读取并检查字符串字段。 */
std::string string_field(const Json& object, const char* key) {
    const Json& value = object.at(key);
    if (!value.is_string()) {
        invalid_meta();
    }
    return value.get<std::string>();
}

/** @brief 读取并检查整数字段。 */
int int_field(const Json& object, const char* key) {
    const Json& value = object.at(key);
    if (!value.is_number_integer()) {
        invalid_meta();
    }
    return value.get<int>();
}

/** @brief 将内部字段类型转换为稳定、可读的 JSON 名称。 */
const char* type_name(ColType type) {
    switch (type) {
        case TYPE_INT:
            return "INT";
        case TYPE_FLOAT:
            return "FLOAT";
        case TYPE_STRING:
            return "STRING";
        default:
            invalid_meta();
    }
}

/** @brief 将 JSON 中的类型名称转换为内部字段类型。 */
ColType read_type(const std::string& type) {
    if (type == "INT") return TYPE_INT;
    if (type == "FLOAT") return TYPE_FLOAT;
    if (type == "STRING") return TYPE_STRING;
    invalid_meta();
}

/**
 * @brief 读取字段定义，并重建所属表和物理偏移。
 * @throws InternalError 字段缺失、类型错误或长度不符合定长布局。
 */
ColMeta read_column(const Json& value, const std::string& table_name, int offset) {
    require_object(value);
    ColMeta column;
    column.tab_name = table_name;
    column.name = string_field(value, "name");
    column.type = read_type(string_field(value, "type"));
    column.len = int_field(value, "length");
    column.offset = offset;

    if (column.name.empty() || column.len <= 0 ||
        (column.type == TYPE_INT && column.len != static_cast<int>(sizeof(int))) ||
        (column.type == TYPE_FLOAT && column.len != static_cast<int>(sizeof(float)))) {
        invalid_meta();
    }
    return column;
}

/**
 * @brief 读取索引列名，并从表字段重建完整索引元数据。
 * @post 参与索引的字段会在 table.cols 中标记为 index。
 * @throws RMDBError 索引为空、重复、引用未知字段或格式错误。
 */
IndexMeta read_index(const Json& value, TabMeta& table) {
    require_object(value);
    const Json& columns = array_field(value, "columns");
    if (columns.empty()) {
        invalid_meta();
    }

    std::vector<std::string> names;
    for (const Json& name_value : columns) {
        if (!name_value.is_string()) {
            invalid_meta();
        }
        std::string name = name_value.get<std::string>();
        // C++20：ranges::find 判断「名字是否已出现」，等价于 find(begin,end,name)!=end。
        if (name.empty() || std::ranges::find(names, name) != names.end()) {
            invalid_meta();
        }
        names.push_back(std::move(name));
    }
    if (table.is_index(names)) {
        invalid_meta();
    }

    IndexMeta index;
    index.tab_name = table.name;
    if (value.contains("unique")) {
        const Json& unique_value = value.at("unique");
        if (!unique_value.is_boolean()) {
            invalid_meta();
        }
        index.unique = unique_value.get<bool>();
    }
    for (const auto& name : names) {
        auto column = table.get_col(name);
        column->index = true;
        index.cols.push_back(*column);
    }
    index.rebuild_derived();
    return index;
}

/**
 * @brief 读取一张表，并按字段顺序重建记录偏移和索引派生字段。
 * @throws InternalError 字段重复、记录过宽或 JSON 格式错误。
 */
TabMeta read_table(const Json& value) {
    require_object(value);
    TabMeta table;
    table.name = string_field(value, "name");
    if (table.name.empty()) {
        invalid_meta();
    }

    const Json& columns = array_field(value, "columns");
    if (columns.empty()) {
        invalid_meta();
    }

    int offset = 0;
    for (const Json& column_value : columns) {
        ColMeta column = read_column(column_value, table.name, offset);
        if (table.is_col(column.name) || column.len > RM_MAX_RECORD_SIZE - offset) {
            invalid_meta();
        }
        offset += column.len;
        table.cols.push_back(std::move(column));
    }

    for (const Json& index_value : array_field(value, "indexes")) {
        table.indexes.push_back(read_index(index_value, table));
    }
    return table;
}

/** @brief 只输出字段不可推导的名称、类型和长度。 */
Json write_column(const ColMeta& column) {
    Json value = Json::object();
    value["name"] = column.name;
    value["type"] = type_name(column.type);
    value["length"] = column.len;
    return value;
}

/**
 * @brief 输出表的最小目录信息。
 * @note tab_name、offset、index、col_num 和 col_tot_len 均在读取时推导，不重复持久化。
 */
Json write_table(const TabMeta& table) {
    Json columns = Json::array();
    for (const auto& column : table.cols) {
        columns.push_back(write_column(column));
    }

    Json indexes = Json::array();
    for (const auto& index : table.indexes) {
        Json names = Json::array();
        for (const auto& column : index.cols) {
            names.push_back(column.name);
        }
        Json value = Json::object();
        value["columns"] = std::move(names);
        value["unique"] = index.unique;
        indexes.push_back(std::move(value));
    }

    Json value = Json::object();
    value["name"] = table.name;
    value["columns"] = std::move(columns);
    value["indexes"] = std::move(indexes);
    return value;
}

}  // namespace

std::ostream& operator<<(std::ostream& output, const DbMeta& db_meta) {
    Json tables = Json::array();
    // C++17 结构化绑定：map 元素是 pair<const Key, T>，可拆成 [key, value]。
    // `_` 表示本处不使用 key（表名已在 TabMeta::name 中）。
    for (const auto& [_, tab] : db_meta.tabs_) {
        tables.push_back(write_table(tab));
    }

    Json root = Json::object();
    root["database"] = db_meta.name_;
    root["tables"] = std::move(tables);
    output << root.dump(2) << '\n';
    return output;
}

std::istream& operator>>(std::istream& input, DbMeta& db_meta) {
    try {
        const Json root = Json::parse(input);
        require_object(root);

        DbMeta parsed;
        parsed.name_ = string_field(root, "database");
        if (parsed.name_.empty()) {
            invalid_meta();
        }

        for (const Json& table_value : array_field(root, "tables")) {
            TabMeta table = read_table(table_value);
            if (parsed.is_table(table.name)) {
                invalid_meta();
            }
            parsed.tabs_.emplace(table.name, std::move(table));
        }
        db_meta = std::move(parsed);
    } catch (const nlohmann::json::exception&) {
        invalid_meta();
    }
    return input;
}
