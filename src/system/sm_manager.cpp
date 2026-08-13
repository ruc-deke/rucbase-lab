// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "sm_manager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <sstream>
#include <system_error>
#include <utility>

#include "common/context.h"
#include "common/wire_result.h"
#include "index/index_manager.h"
#include "record/rm_manager.h"
#include "storage/buffer_pool_manager.h"
#include "storage/disk_manager.h"

namespace {

namespace fs = std::filesystem;

/**
 * @brief 将数据库名约束为当前目录下的单个路径分量。
 * @throws RMDBError 名称为空、包含父目录或包含路径分隔符。
 */
fs::path database_path(const std::string& db_name) {
    fs::path path(db_name);
    if (path.empty() || path != path.filename() || path == "." || path == "..") {
        throw RMDBError("Invalid database name: " + db_name);
    }
    return path;
}

/**
 * @brief 序列化后再覆盖元数据文件，避免序列化失败时提前清空旧文件。
 * @throws InternalError 序列化、打开或写入失败。
 */
void write_meta_file(const fs::path& path, const DbMeta& db) {
    std::ostringstream serialized;
    serialized << db;
    if (!serialized) {
        throw InternalError("cannot serialize database metadata");
    }

    std::ofstream output(path, std::ios::out | std::ios::trunc);
    if (!output) {
        throw InternalError("cannot open database metadata: " + path.string());
    }
    output << serialized.str();
    output.flush();
    if (!output) {
        throw InternalError("cannot write database metadata: " + path.string());
    }
}

/**
 * @brief 将系统命令的字符串结果转换成统一的 WireResultSet。
 * @throws InternalError 上下文为空、行列不匹配或结果超过教学版缓存上限。
 */
void set_string_result(Context* context,
                       std::vector<std::string> column_names,
                       std::vector<std::vector<std::string>> row_values) {
    if (context == nullptr) {
        throw InternalError("utility result requires a request context");
    }

    std::vector<WireResultColumn> columns;
    columns.reserve(column_names.size());
    for (auto& name : column_names) {
        columns.push_back({.name = std::move(name), .type = TYPE_STRING});
    }
    context->result().set_columns(std::move(columns));

    for (auto& values : row_values) {
        std::vector<WireResultCell> row;
        row.reserve(values.size());
        for (auto& value : values) {
            row.push_back({.type = TYPE_STRING, .str_val = std::move(value)});
        }
        context->result().add_row(std::move(row));
    }
}

}  // namespace

bool SmManager::is_dir(const std::string& db_name) const { return fs::is_directory(database_path(db_name)); }

void SmManager::create_db(const std::string& db_name) const {
    const fs::path db_path = database_path(db_name);
    if (fs::exists(db_path)) {
        throw DatabaseExistsError(db_name);
    }

    if (!fs::create_directory(db_path)) {
        throw DatabaseExistsError(db_name);
    }

    try {
        DbMeta new_db;
        new_db.name_ = db_name;
        write_meta_file(db_path / DB_META_NAME, new_db);
        disk_manager_->create_file((db_path / LOG_FILE_NAME).string());
    } catch (...) {
        // 只回滚本次刚创建的目录，不触碰任何既有路径。
        std::error_code ignored;
        fs::remove_all(db_path, ignored);
        throw;
    }
}

void SmManager::drop_db(const std::string& db_name) {
    const fs::path db_path = database_path(db_name);
    if (!fs::is_directory(db_path) || !fs::is_regular_file(db_path / DB_META_NAME)) {
        throw DatabaseNotFoundError(db_name);
    }
    if (db_.name_ == db_name) {
        throw InternalError("cannot drop an open database: " + db_name);
    }
    if (fs::remove_all(db_path) == 0) {
        throw DatabaseNotFoundError(db_name);
    }
}

void SmManager::open_db(const std::string& db_name) {
    // TODO(Lab 3): 加载数据库元数据，并打开所有表文件和索引文件。
    throw NotImplementedError("SmManager::open_db (Lab 3)");
}

void SmManager::flush_meta() const { write_meta_file(DB_META_NAME, db_); }

void SmManager::show_database(Context* context) const { set_string_result(context, {"Database"}, {{db_.name_}}); }

void SmManager::close_db() {
    // TODO(Lab 3): 刷盘并关闭所有表、索引和数据库元数据。
    throw NotImplementedError("SmManager::close_db (Lab 3)");
}

void SmManager::show_tables(Context* context) {
    std::vector<std::vector<std::string>> rows;
    rows.reserve(db_.tabs_.size());
    // C++20：`map | std::views::values` 只遍历 value（表元数据），不关心 key。
    // 等价于 for (const auto& [name, tab] : db_.tabs_) { ... 使用 tab ... }
    for (const auto& tab : db_.tabs_ | std::views::values) {
        rows.push_back({tab.name});
    }
    set_string_result(context, {"Tables"}, std::move(rows));
}

void SmManager::desc_table(const std::string& tab_name, Context* context) {
    const TabMeta& tab = db_.get_table(tab_name);

    std::vector<std::vector<std::string>> rows;
    rows.reserve(tab.cols.size());
    for (const auto& col : tab.cols) {
        rows.push_back({col.name, coltype2str(col.type), col.index ? "YES" : "NO"});
    }
    set_string_result(context, {"Field", "Type", "Index"}, std::move(rows));
}

void SmManager::create_table(const std::string& tab_name, const std::vector<ColDef>& col_defs, Context* context) {
    if (db_.is_table(tab_name)) {
        throw TableExistsError(tab_name);
    }
    // C++20：unordered_map::contains，避免 find/end 比较。
    if (fhs_.contains(tab_name)) {
        throw InternalError("Record handle already exists: " + tab_name);
    }
    if (col_defs.empty()) {
        throw InvalidRecordSizeError(0);
    }

    std::vector<std::string> col_names;
    col_names.reserve(col_defs.size());
    int record_size = 0;
    // C++17 结构化绑定：ColDef 是聚合类型，可拆成 name/type/len。
    // 等价于 for (const auto& col_def : col_defs) { 使用 col_def.name 等 }
    for (const auto& [name, type, len] : col_defs) {
        // C++20 ranges::find：直接对容器查找，无需 begin()/end()。
        if (std::ranges::find(col_names, name) != col_names.end()) {
            throw ColumnExistsError(name);
        }
        col_names.push_back(name);

        const bool fixed_length_mismatch = (type == TYPE_INT && len != static_cast<int>(sizeof(int))) ||
                                           (type == TYPE_FLOAT && len != static_cast<int>(sizeof(float)));
        if (len <= 0 || fixed_length_mismatch) {
            throw InvalidColLengthError(len);
        }
        if (len > RM_MAX_RECORD_SIZE) {
            throw InvalidRecordSizeError(len);
        }
        const int next_record_size = record_size + len;
        if (next_record_size > RM_MAX_RECORD_SIZE) {
            throw InvalidRecordSizeError(next_record_size);
        }
        record_size = next_record_size;
    }

    // 字段按声明顺序连续存放，offset 是字段在一条记录中的起始位置。
    int curr_offset = 0;
    TabMeta tab;
    tab.name = tab_name;
    tab.cols.reserve(col_defs.size());
    // 同上：结构化绑定展开 ColDef 的三个成员。
    for (const auto& [name, type, len] : col_defs) {
        ColMeta col;
        col.tab_name = tab_name;
        col.name = name;
        col.type = type;
        col.len = len;
        col.offset = curr_offset;
        curr_offset += len;
        tab.cols.push_back(std::move(col));
    }

    // 先构造候选目录，只有文件句柄和 db.meta 都准备成功后才替换当前目录。
    DbMeta next_db = db_;
    next_db.tabs_[tab_name] = tab;
    rm_manager_->create_file(tab_name, record_size);

    try {
        fhs_.emplace(tab_name, rm_manager_->open_file(tab_name));
        write_meta_file(DB_META_NAME, next_db);
        db_ = std::move(next_db);
    } catch (...) {
        // 只清理本次建表产生的句柄和文件，再保留原目录供调用者继续使用。
        if (auto handle = fhs_.find(tab_name); handle != fhs_.end()) {
            rm_manager_->close_file(handle->second.get());
            fhs_.erase(handle);
        }
        rm_manager_->destroy_file(tab_name);
        throw;
    }
}

void SmManager::drop_table(const std::string& tab_name, Context* context) {
    // TODO(Lab 3): 删除表的索引、记录文件、句柄和元数据。
    throw NotImplementedError("SmManager::drop_table (Lab 3)");
}

void SmManager::create_index(const std::string& tab_name,
                             const std::vector<std::string>& col_names,
                             [[maybe_unused]] bool unique,
                             Context* context) {
    // TODO(Lab 3): 用 IndexMeta::make(表, 列, unique) 构造定义，再交给 IndexManager::create_index。
    throw NotImplementedError("SmManager::create_index (Lab 3)");
}

void SmManager::drop_index(const std::string& tab_name, const std::vector<std::string>& col_names, Context* context) {
    // TODO(Lab 3): 关闭并删除索引，同时维护索引元数据。
    throw NotImplementedError("SmManager::drop_index (Lab 3)");
}

void SmManager::drop_index(const std::string& tab_name, const std::vector<ColMeta>& cols, Context* context) {
    // TODO(Lab 3): 与按字段名删除索引的重载共享核心逻辑。
    throw NotImplementedError("SmManager::drop_index (Lab 3)");
}
