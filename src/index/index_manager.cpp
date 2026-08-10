// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "index_manager.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "b_plus_tree.h"
#include "common/config.h"
#include "common/defs.h"
#include "common/errors.h"
#include "index_types.h"
#include "storage/buffer_pool_manager.h"
#include "storage/disk_manager.h"
#include "system/sm_meta.h"

IndexManager::IndexManager(DiskManager* disk_manager, BufferPoolManager* buffer_pool_manager)
    : disk_manager_(disk_manager),
      buffer_pool_manager_(buffer_pool_manager) {}

std::string IndexManager::make_index_name(const std::string& table_name,
                                          const std::vector<std::string>& index_columns) const {
    std::string index_name = table_name;
    for (const auto& column : index_columns) {
        index_name += "_" + column;
    }
    return index_name + ".idx";
}

std::string IndexManager::make_index_name(const std::string& table_name,
                                          const std::vector<ColMeta>& index_columns) const {
    std::string index_name = table_name;
    for (const auto& column : index_columns) {
        index_name += "_" + column.name;
    }
    return index_name + ".idx";
}

bool IndexManager::exists(const std::string& table_name, const std::vector<ColMeta>& index_columns) const {
    return disk_manager_->is_file(make_index_name(table_name, index_columns));
}

bool IndexManager::exists(const std::string& table_name, const std::vector<std::string>& index_columns) const {
    return disk_manager_->is_file(make_index_name(table_name, index_columns));
}

void IndexManager::create_index(const std::string& table_name, const std::vector<ColMeta>& index_columns) {
    // 为插入和删除临时多保留一个槽位：header + (key + Rid) * (order + 1) <= PAGE_SIZE。
    if (index_columns.empty() || index_columns.size() > IndexFileHeader::max_serialized_columns()) {
        throw InternalError("Invalid number of index columns");
    }

    int64_t checked_key_length = 0;
    for (const auto& column : index_columns) {
        bool valid_length = column.len > 0;
        switch (column.type) {
            case TYPE_INT:
                valid_length = valid_length && column.len == static_cast<int>(sizeof(int));
                break;
            case TYPE_FLOAT:
                valid_length = valid_length && column.len == static_cast<int>(sizeof(float));
                break;
            case TYPE_STRING:
                break;
            default:
                throw InternalError("Unexpected data type");
        }
        if (!valid_length) {
            throw InvalidColLengthError(column.len);
        }
        checked_key_length += column.len;
        if (checked_key_length > INDEX_MAX_KEY_LENGTH) {
            throw InvalidColLengthError(static_cast<int>(checked_key_length));
        }
    }

    const int column_count = static_cast<int>(index_columns.size());
    const int key_length = static_cast<int>(checked_key_length);
    int tree_order = static_cast<int>((PAGE_SIZE - sizeof(IndexPageHeader)) / (key_length + sizeof(Rid)) - 1);
    while (tree_order > 2 && !IndexFileHeader::page_layout_fits(tree_order, key_length)) {
        --tree_order;
    }
    if (tree_order <= 2 || !IndexFileHeader::page_layout_fits(tree_order, key_length)) {
        throw InvalidColLengthError(key_length);
    }

    const int keys_region_size = IndexFileHeader::calculate_keys_region_size(tree_order, key_length);
    IndexFileHeader file_header(INDEX_NO_PAGE, INDEX_INITIAL_PAGE_COUNT, INDEX_INITIAL_ROOT_PAGE, column_count,
                                key_length, tree_order, keys_region_size, INDEX_INITIAL_ROOT_PAGE,
                                INDEX_INITIAL_ROOT_PAGE);
    for (const auto& column : index_columns) {
        file_header.column_types_.push_back(column.type);
        file_header.column_lengths_.push_back(column.len);
    }
    file_header.update_serialized_size();

    std::vector<char> header_data(file_header.serialized_size_);
    file_header.serialize(header_data.data());

    const std::string index_name = make_index_name(table_name, index_columns);
    disk_manager_->create_file(index_name);
    const int file_descriptor = disk_manager_->open_file(index_name);
    disk_manager_->write_page(file_descriptor, INDEX_FILE_HEADER_PAGE, header_data.data(),
                              file_header.serialized_size_);

    alignas(IndexPageHeader) char page_buffer[PAGE_SIZE];
    std::memset(page_buffer, 0, PAGE_SIZE);

    // 1 号页是叶子链表哨兵，前后均指向初始根节点。
    auto* leaf_header = reinterpret_cast<IndexPageHeader*>(page_buffer);
    leaf_header->next_free_page = INDEX_NO_PAGE;
    leaf_header->parent_page = INDEX_NO_PAGE;
    leaf_header->key_count = 0;
    leaf_header->is_leaf = true;
    leaf_header->previous_leaf_page = INDEX_INITIAL_ROOT_PAGE;
    leaf_header->next_leaf_page = INDEX_INITIAL_ROOT_PAGE;
    disk_manager_->write_page(file_descriptor, INDEX_LEAF_HEADER_PAGE, page_buffer, PAGE_SIZE);

    // 2 号页是初始根节点，同时也是空树唯一的叶节点。
    std::memset(page_buffer, 0, PAGE_SIZE);
    auto* root_header = reinterpret_cast<IndexPageHeader*>(page_buffer);
    root_header->next_free_page = INDEX_NO_PAGE;
    root_header->parent_page = INDEX_NO_PAGE;
    root_header->key_count = 0;
    root_header->is_leaf = true;
    root_header->previous_leaf_page = INDEX_LEAF_HEADER_PAGE;
    root_header->next_leaf_page = INDEX_LEAF_HEADER_PAGE;
    disk_manager_->write_page(file_descriptor, INDEX_INITIAL_ROOT_PAGE, page_buffer, PAGE_SIZE);

    disk_manager_->set_fd2pageno(file_descriptor, INDEX_INITIAL_PAGE_COUNT);
    disk_manager_->close_file(file_descriptor);
}

void IndexManager::destroy_index(const std::string& table_name, const std::vector<ColMeta>& index_columns) {
    disk_manager_->destroy_file(make_index_name(table_name, index_columns));
}

void IndexManager::destroy_index(const std::string& table_name, const std::vector<std::string>& index_columns) {
    disk_manager_->destroy_file(make_index_name(table_name, index_columns));
}

std::unique_ptr<BPlusTree> IndexManager::open_index(const std::string& table_name,
                                                    const std::vector<ColMeta>& index_columns) {
    const std::string index_name = make_index_name(table_name, index_columns);
    const int file_descriptor = disk_manager_->open_file(index_name);
    try {
        return std::make_unique<BPlusTree>(disk_manager_, buffer_pool_manager_, file_descriptor);
    } catch (...) {
        disk_manager_->close_file(file_descriptor);
        throw;
    }
}

std::unique_ptr<BPlusTree> IndexManager::open_index(const std::string& table_name,
                                                    const std::vector<std::string>& index_columns) {
    const std::string index_name = make_index_name(table_name, index_columns);
    const int file_descriptor = disk_manager_->open_file(index_name);
    try {
        return std::make_unique<BPlusTree>(disk_manager_, buffer_pool_manager_, file_descriptor);
    } catch (...) {
        disk_manager_->close_file(file_descriptor);
        throw;
    }
}

void IndexManager::close_index(const BPlusTree* index) {
    std::vector<char> header_data(index->file_header_->serialized_size_);
    index->file_header_->serialize(header_data.data());
    disk_manager_->write_page(index->file_descriptor_, INDEX_FILE_HEADER_PAGE, header_data.data(),
                              index->file_header_->serialized_size_);
    // 必须先刷新缓存页，再关闭底层文件描述符。
    buffer_pool_manager_->flush_all_pages(index->file_descriptor_);
    disk_manager_->close_file(index->file_descriptor_);
}
