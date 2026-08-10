// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cstddef>
#include <cstring>
#include <mutex>
#include <utility>
#include <vector>

#include "common/config.h"
#include "common/defs.h"
#include "index_types.h"

class BufferPoolManager;
class DiskManager;
class Page;
struct PageId;
class Transaction;

/** @brief 查找叶节点时的操作类型；写操作后续需要配合并发控制。 */
enum class IndexOperation { Find, Insert, Delete };

/** @brief 按字段类型比较两个定长索引键。 */
int compare_index_key(const char* left, const char* right, ColType type, int column_length);

/** @brief 按列顺序比较两个复合索引键。 */
int compare_index_key(const char* left,
                      const char* right,
                      const std::vector<ColType>& column_types,
                      const std::vector<int>& column_lengths);

/**
 * @brief 将一个已固定的缓冲池页解释为 B+ 树节点。
 * @note 该对象不拥有 Page，使用完后必须由 BPlusTree 解固定对应页。
 */
class BPlusTreeNode {
    friend class BPlusTree;
    friend class IndexScan;

private:
    const IndexFileHeader* file_header_;  // 节点所在索引文件的元数据
    Page* page_;                          // 缓冲池中固定的页面
    IndexPageHeader* page_header_;        // 页面头
    char* keys_;                          // 连续存放的定长键
    Rid* rids_;                           // 与键位置一一对应的 Rid

public:
    BPlusTreeNode() = default;

    BPlusTreeNode(const IndexFileHeader* file_header, Page* page);

    int get_size() const noexcept { return page_header_->key_count; }

    void set_size(int size) { page_header_->key_count = size; }

    int get_max_size() const noexcept { return file_header_->tree_order_ + 1; }

    int get_min_size() const noexcept { return get_max_size() / 2; }

    int key_at(int i) const {
        int key;
        memcpy(&key, get_key(i), sizeof(key));
        return key;
    }

    /* 得到第i个孩子结点的page_no */
    page_id_t value_at(int i) const noexcept { return get_rid(i)->page_no; }

    page_id_t get_page_no() const noexcept;

    PageId get_page_id() const noexcept;

    page_id_t get_next_leaf() const noexcept { return page_header_->next_leaf_page; }

    page_id_t get_prev_leaf() const noexcept { return page_header_->previous_leaf_page; }

    page_id_t get_parent_page_no() const noexcept { return page_header_->parent_page; }

    bool is_leaf_page() const noexcept { return page_header_->is_leaf; }

    bool is_root_page() const noexcept { return get_parent_page_no() == INDEX_NO_PAGE; }

    void set_next_leaf(page_id_t page_no) { page_header_->next_leaf_page = page_no; }

    void set_prev_leaf(page_id_t page_no) { page_header_->previous_leaf_page = page_no; }

    void set_parent_page_no(page_id_t parent) { page_header_->parent_page = parent; }

    char* get_key(int key_idx) const {
        return keys_ + static_cast<std::ptrdiff_t>(key_idx) * file_header_->key_length_;
    }

    Rid* get_rid(int rid_idx) const { return &rids_[rid_idx]; }

    void set_key(int key_idx, const char* key) {
        memcpy(keys_ + static_cast<std::ptrdiff_t>(key_idx) * file_header_->key_length_, key,
               file_header_->key_length_);
    }

    void set_rid(int rid_idx, const Rid& rid) { rids_[rid_idx] = rid; }

    int lower_bound(const char* target) const;

    int upper_bound(const char* target) const;

    void insert_pairs(int pos, const char* key, const Rid* rid, int n);

    page_id_t internal_lookup(const char* key);

    bool leaf_lookup(const char* key, Rid** value);

    int insert(const char* key, const Rid& value);

    // 用于在结点中的指定位置插入单个键值对
    void insert_pair(int pos, const char* key, const Rid& rid) { insert_pairs(pos, key, &rid, 1); }

    void erase_pair(int pos);

    int remove(const char* key);

    /**
     * @brief used in internal node to remove the last key in root node, and return the last child
     *
     * @return the last child
     */
    page_id_t remove_and_return_only_child();

    /**
     * @brief 由parent调用，寻找child，返回child在parent中的rid_idx∈[0,page_hdr->key_count)
     * @param child
     * @return int
     */
    int find_child(const BPlusTreeNode* child) const;
};

/**
 * @brief 管理一个索引文件中的 B+ 树。
 * @note 拥有 IndexFileHeader，但不拥有磁盘管理器和缓冲池管理器。
 */
class BPlusTree {
    friend class IndexScan;
    friend class IndexManager;

private:
    DiskManager* disk_manager_;               ///< 非拥有指针。
    BufferPoolManager* buffer_pool_manager_;  ///< 非拥有指针。
    int file_descriptor_;                     ///< 索引文件描述符。
    IndexFileHeader* file_header_;            ///< 拥有的内存文件头副本。
    std::mutex root_latch_;

public:
    BPlusTree(DiskManager* disk_manager, BufferPoolManager* buffer_pool_manager, int file_descriptor);

    ~BPlusTree();

    // for search
    bool get_value(const char* key, std::vector<Rid>* result, Transaction* transaction);

    std::pair<BPlusTreeNode*, bool> find_leaf_page(const char* key,
                                                   IndexOperation operation,
                                                   Transaction* transaction,
                                                   bool find_first = false);

    // for insert
    page_id_t insert_entry(const char* key, const Rid& value, Transaction* transaction);

    BPlusTreeNode* split(BPlusTreeNode* node);

    void insert_into_parent(BPlusTreeNode* old_node,
                            const char* key,
                            BPlusTreeNode* new_node,
                            Transaction* transaction);

    // for delete
    bool delete_entry(const char* key, Transaction* transaction);

    bool coalesce_or_redistribute(BPlusTreeNode* node,
                                  Transaction* transaction = nullptr,
                                  bool* root_is_latched = nullptr);
    bool adjust_root(BPlusTreeNode* old_root_node);

    void redistribute(BPlusTreeNode* neighbor_node, BPlusTreeNode* node, BPlusTreeNode* parent, int index);

    bool coalesce(BPlusTreeNode** neighbor_node,
                  BPlusTreeNode** node,
                  BPlusTreeNode** parent,
                  int index,
                  Transaction* transaction,
                  bool* root_is_latched);

    IndexPosition lower_bound(const char* key);

    IndexPosition upper_bound(const char* key);

    IndexPosition leaf_end() const;

    IndexPosition leaf_begin() const;

private:
    // 辅助函数
    void set_root_page(page_id_t root) { file_header_->root_page_ = root; }

    bool is_empty() const { return file_header_->root_page_ == INDEX_NO_PAGE; }

    // for get/create node
    BPlusTreeNode* fetch_node(int page_no) const;
    void unpin_node(BPlusTreeNode* node, bool dirty) const;

    BPlusTreeNode* create_node();

    // for maintain data structure
    void update_ancestor_keys(BPlusTreeNode* node);

    void unlink_leaf(BPlusTreeNode* leaf);

    void record_page_deletion();

    void update_child_parent(BPlusTreeNode* node, int child_idx);

    // for index test
    Rid get_rid(const IndexPosition& position) const;
};
