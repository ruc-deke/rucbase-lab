// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

/**
 * @file index_manager.h
 * @brief 定义索引文件的创建、打开、关闭和删除接口。
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "system/sm_meta.h"

class BPlusTree;
class BufferPoolManager;
class DiskManager;

/**
 * @brief 管理索引文件的生命周期。
 *
 * IndexManager 不实现 B+ 树算法，只负责准备索引文件、持久化文件头，
 * 并为已打开的索引创建 BPlusTree 对象。
 */
class IndexManager {
public:
    IndexManager(DiskManager* disk_manager, BufferPoolManager* buffer_pool_manager);

    std::string make_index_name(const std::string& table_name, const std::vector<std::string>& index_columns) const;
    std::string make_index_name(const std::string& table_name, const std::vector<ColMeta>& index_columns) const;

    bool exists(const std::string& table_name, const std::vector<ColMeta>& index_columns) const;
    bool exists(const std::string& table_name, const std::vector<std::string>& index_columns) const;

    void create_index(const IndexMeta& index);

    void destroy_index(const std::string& table_name, const std::vector<ColMeta>& index_columns);
    void destroy_index(const std::string& table_name, const std::vector<std::string>& index_columns);

    std::unique_ptr<BPlusTree> open_index(const std::string& table_name, const std::vector<ColMeta>& index_columns);
    std::unique_ptr<BPlusTree> open_index(const std::string& table_name, const std::vector<std::string>& index_columns);

    void close_index(const BPlusTree* index);

private:
    DiskManager* disk_manager_;               ///< 非拥有指针。
    BufferPoolManager* buffer_pool_manager_;  ///< 非拥有指针。
};
