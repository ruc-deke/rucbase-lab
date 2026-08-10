// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "index/b_plus_tree.h"
#include "record/rm_file_handle.h"
#include "sm_defs.h"
#include "sm_meta.h"

class BufferPoolManager;
class Context;
class DiskManager;
class IndexManager;
class RmManager;

/**
 * @brief 管理当前数据库的元数据，并执行数据库、表和索引的 DDL 操作。
 *
 * SmManager 不拥有构造函数传入的底层管理器。公开的元数据和句柄用于各实验模块协作，
 * 因此修改 DDL 时必须保持磁盘文件、文件句柄和元数据三者一致。
 * 教学版本不支持 DDL 与其他 SQL 并发执行。
 */
class SmManager {
public:
    DbMeta db_;                                                            ///< 当前打开数据库的元数据。
    std::unordered_map<std::string, std::unique_ptr<RmFileHandle>> fhs_;   ///< 表名 -> 记录文件句柄。
    std::unordered_map<std::string, std::unique_ptr<BPlusTree>> indexes_;  ///< 索引文件名 -> 索引句柄。

private:
    DiskManager* disk_manager_;               ///< 非拥有指针。
    BufferPoolManager* buffer_pool_manager_;  ///< 非拥有指针。
    RmManager* rm_manager_;                   ///< 非拥有指针。
    IndexManager* index_manager_;                   ///< 非拥有指针。

public:
    /**
     * @brief 构造系统管理器。
     * @param disk_manager 磁盘管理器，非拥有指针。
     * @param buffer_pool_manager 缓冲池管理器，非拥有指针。
     * @param rm_manager 记录管理器，非拥有指针。
     * @param index_manager 索引管理器，非拥有指针。
     */
    SmManager(DiskManager* disk_manager,
              BufferPoolManager* buffer_pool_manager,
              RmManager* rm_manager,
              IndexManager* index_manager)
        : disk_manager_(disk_manager),
          buffer_pool_manager_(buffer_pool_manager),
          rm_manager_(rm_manager),
          index_manager_(index_manager) {}

    ~SmManager() = default;

    /** @brief 获取缓冲池管理器。 */
    BufferPoolManager* get_bpm() const noexcept { return buffer_pool_manager_; }

    /** @brief 获取记录管理器。 */
    RmManager* get_rm_manager() const noexcept { return rm_manager_; }

    /** @brief 获取索引管理器。 */
    IndexManager* get_index_manager() const noexcept { return index_manager_; }

    /** @brief 判断数据库目录是否存在。 */
    bool is_dir(const std::string& db_name) const;

    /** @brief 创建数据库目录、元数据文件和日志文件。 */
    void create_db(const std::string& db_name) const;

    /** @brief 删除一个未打开的数据库及其文件。 */
    void drop_db(const std::string& db_name);

    /**
     * @brief 打开数据库并加载表、索引的元数据和文件句柄。
     * @post 当前工作目录保持在该数据库目录中，直到 close_db 返回。
     * @todo Lab 3：由学生实现数据库打开流程。
     */
    void open_db(const std::string& db_name);

    /**
     * @brief 刷盘并关闭当前数据库。
     * @post 当前工作目录回到数据库目录的上一级。
     * @todo Lab 3：由学生实现数据库关闭流程。
     */
    void close_db();

    /** @brief 将当前数据库元数据写入磁盘。 @pre 当前工作目录是已打开的数据库目录。 */
    void flush_meta() const;

    /** @brief 返回当前打开的数据库名。 @pre context != nullptr */
    void show_database(Context* context) const;

    /** @brief 返回当前数据库中的所有表。 @pre context != nullptr */
    void show_tables(Context* context);

    /** @brief 返回指定表的字段信息。 @pre context != nullptr */
    void desc_table(const std::string& tab_name, Context* context);

    /**
     * @brief 创建表及其记录文件。
     * @param tab_name 待创建的表名。
     * @param col_defs 按声明顺序排列的字段定义。
     * @param context 当前请求上下文；Lab 3 的部分单元测试可能传入 nullptr。
     */
    void create_table(const std::string& tab_name, const std::vector<ColDef>& col_defs, Context* context);

    /**
     * @brief 删除表及其记录文件和索引。
     * @todo Lab 3：由学生实现删表流程。
     */
    void drop_table(const std::string& tab_name, Context* context);

    /**
     * @brief 为指定字段创建索引。
     * @todo Lab 3：由学生实现索引创建流程。
     */
    void create_index(const std::string& tab_name, const std::vector<std::string>& col_names, Context* context);

    /**
     * @brief 按字段名删除索引。
     * @todo Lab 3：由学生实现索引删除流程。
     */
    void drop_index(const std::string& tab_name, const std::vector<std::string>& col_names, Context* context);

    /**
     * @brief 按字段元数据删除索引，供内部 DDL 流程复用。
     * @todo Lab 3：与按字段名删除索引共享同一套核心逻辑。
     */
    void drop_index(const std::string& tab_name, const std::vector<ColMeta>& cols, Context* context);
};
