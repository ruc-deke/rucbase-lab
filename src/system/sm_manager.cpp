// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "sm_manager.h"

#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <fstream>
#include <utility>

#include "index/ix.h"
#include "record/rm.h"

namespace {

void SetStringResult(Context* context, std::vector<std::string> column_names,
                     std::vector<std::vector<std::string>> row_values) {
    WireResultSet result;
    result.has_query_result = true;
    result.columns.reserve(column_names.size());
    for (auto& name : column_names) {
        result.columns.push_back({std::move(name), TYPE_STRING});
    }
    size_t schema_bytes = result.columns.size() * sizeof(WireResultColumn);
    if (schema_bytes > WireResultSet::kMaxBufferedBytes) {
        throw InternalError("utility result schema exceeds the 16 MiB teaching wire buffer");
    }
    for (const auto& column : result.columns) {
        if (column.name.size() > WireResultSet::kMaxBufferedBytes - schema_bytes) {
            throw InternalError("utility result schema exceeds the 16 MiB teaching wire buffer");
        }
        schema_bytes += column.name.size();
    }
    if (!result.try_account(schema_bytes)) {
        throw InternalError("utility result schema exceeds the 16 MiB teaching wire buffer");
    }

    if (row_values.size() > WireResultSet::kMaxBufferedBytes /
                                (2 * sizeof(std::vector<WireResultCell>))) {
        throw InternalError("utility result exceeds the 16 MiB teaching wire buffer");
    }
    result.rows.reserve(row_values.size());
    for (auto& values : row_values) {
        if (values.size() != result.columns.size()) {
            throw InternalError("utility result row does not match its columns");
        }

        std::vector<WireResultCell> row;
        size_t row_bytes = 2 * sizeof(std::vector<WireResultCell>) +
                           values.size() * sizeof(WireResultCell);
        if (row_bytes > WireResultSet::kMaxBufferedBytes) {
            throw InternalError("utility result exceeds the 16 MiB teaching wire buffer");
        }
        row.reserve(values.size());
        for (auto& value : values) {
            if (value.size() > WireResultSet::kMaxBufferedBytes - row_bytes) {
                throw InternalError("utility result exceeds the 16 MiB teaching wire buffer");
            }
            row_bytes += value.size();

            WireResultCell cell;
            cell.type = TYPE_STRING;
            cell.str_val = std::move(value);
            row.push_back(std::move(cell));
        }
        if (!result.try_account(row_bytes)) {
            throw InternalError("utility result exceeds the 16 MiB teaching wire buffer");
        }
        result.rows.push_back(std::move(row));
    }

    context->wire_result_ = std::move(result);
}

}  // namespace

/**
 * @description: 判断是否为一个文件夹
 * @return {bool} 返回是否为一个文件夹
 * @param {string&} db_name 数据库文件名称，与文件夹同名
 */
bool SmManager::is_dir(const std::string& db_name) {
    struct stat st;
    return stat(db_name.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

/**
 * @description: 创建数据库，所有的数据库相关文件都放在数据库同名文件夹下
 * @param {string&} db_name 数据库名称
 */
void SmManager::create_db(const std::string& db_name) {
    if (is_dir(db_name)) {
        throw DatabaseExistsError(db_name);
    }
    //为数据库创建一个子目录
    std::string cmd = "mkdir " + db_name;
    if (system(cmd.c_str()) < 0) {  // 创建一个名为db_name的目录
        throw UnixError();
    }
    if (chdir(db_name.c_str()) < 0) {  // 进入名为db_name的目录
        throw UnixError();
    }
    //创建系统目录
    DbMeta *new_db = new DbMeta();
    new_db->name_ = db_name;

    // 注意，此处ofstream会在当前目录创建(如果没有此文件先创建)和打开一个名为DB_META_NAME的文件
    std::ofstream ofs(DB_META_NAME);

    // 将new_db中的信息，按照定义好的operator<<操作符，写入到ofs打开的DB_META_NAME文件中
    ofs << *new_db;  // 注意：此处重载了操作符<<

    delete new_db;

    // 创建日志文件
    disk_manager_->create_file(LOG_FILE_NAME);

    // 回到根目录
    if (chdir("..") < 0) {
        throw UnixError();
    }
}

/**
 * @description: 删除数据库，同时需要清空相关文件以及数据库同名文件夹
 * @param {string&} db_name 数据库名称，与文件夹同名
 */
void SmManager::drop_db(const std::string& db_name) {
    if (!is_dir(db_name)) {
        throw DatabaseNotFoundError(db_name);
    }
    std::string cmd = "rm -r " + db_name;
    if (system(cmd.c_str()) < 0) {
        throw UnixError();
    }
}

/**
 * @description: 打开数据库，找到数据库对应的文件夹，并加载数据库元数据和相关文件
 * @param {string&} db_name 数据库名称，与文件夹同名
 */
void SmManager::open_db(const std::string& db_name) {
    
}

/**
 * @description: 把数据库相关的元数据刷入磁盘中
 */
void SmManager::flush_meta() {
    // 默认清空文件
    std::ofstream ofs(DB_META_NAME);
    ofs << db_;
}

/**
 * @description: 关闭数据库并把数据落盘
 */
void SmManager::close_db() {
    
}

/**
 * @description: 显示所有的表，结果通过当前请求的 Wire 响应返回
 * @param {Context*} context 
 */
void SmManager::show_tables(Context* context) {
    std::vector<std::vector<std::string>> rows;
    rows.reserve(db_.tabs_.size());
    for (const auto& entry : db_.tabs_) {
        rows.push_back({entry.second.name});
    }
    SetStringResult(context, {"Tables"}, std::move(rows));
}

/**
 * @description: 显示表的元数据
 * @param {string&} tab_name 表名称
 * @param {Context*} context 
 */
void SmManager::desc_table(const std::string& tab_name, Context* context) {
    const TabMeta& tab = db_.get_table(tab_name);

    std::vector<std::vector<std::string>> rows;
    rows.reserve(tab.cols.size());
    for (const auto& col : tab.cols) {
        rows.push_back({col.name, coltype2str(col.type), col.index ? "YES" : "NO"});
    }
    SetStringResult(context, {"Field", "Type", "Index"}, std::move(rows));
}

/**
 * @description: 创建表
 * @param {string&} tab_name 表的名称
 * @param {vector<ColDef>&} col_defs 表的字段
 * @param {Context*} context 
 */
void SmManager::create_table(const std::string& tab_name, const std::vector<ColDef>& col_defs, Context* context) {
    if (db_.is_table(tab_name)) {
        throw TableExistsError(tab_name);
    }
    // Create table meta
    int curr_offset = 0;
    TabMeta tab;
    tab.name = tab_name;
    for (auto &col_def : col_defs) {
        ColMeta col = {.tab_name = tab_name,
                       .name = col_def.name,
                       .type = col_def.type,
                       .len = col_def.len,
                       .offset = curr_offset,
                       .index = false};
        curr_offset += col_def.len;
        tab.cols.push_back(col);
    }
    // Create & open record file
    int record_size = curr_offset;  // record_size就是col meta所占的大小（表的元数据也是以记录的形式进行存储的）
    rm_manager_->create_file(tab_name, record_size);
    db_.tabs_[tab_name] = tab;
    // fhs_[tab_name] = rm_manager_->open_file(tab_name);
    fhs_.emplace(tab_name, rm_manager_->open_file(tab_name));

    flush_meta();
}

/**
 * @description: 删除表
 * @param {string&} tab_name 表的名称
 * @param {Context*} context
 */
void SmManager::drop_table(const std::string& tab_name, Context* context) {
    
}

/**
 * @description: 创建索引
 * @param {string&} tab_name 表的名称
 * @param {vector<string>&} col_names 索引包含的字段名称
 * @param {Context*} context
 */
void SmManager::create_index(const std::string& tab_name, const std::vector<std::string>& col_names, Context* context) {
    
}

/**
 * @description: 删除索引
 * @param {string&} tab_name 表名称
 * @param {vector<string>&} col_names 索引包含的字段名称
 * @param {Context*} context
 */
void SmManager::drop_index(const std::string& tab_name, const std::vector<std::string>& col_names, Context* context) {
    
}

/**
 * @description: 删除索引
 * @param {string&} tab_name 表名称
 * @param {vector<ColMeta>&} 索引包含的字段元数据
 * @param {Context*} context
 */
void SmManager::drop_index(const std::string& tab_name, const std::vector<ColMeta>& cols, Context* context) {
    
}
