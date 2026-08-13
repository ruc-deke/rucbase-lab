// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "common/config.h"
#include "common/defs.h"

/** @file index_types.h @brief 定义索引页布局、文件头和扫描位置。 */

constexpr int INDEX_NO_PAGE = -1;
constexpr page_id_t INDEX_FILE_HEADER_PAGE = 0;
constexpr page_id_t INDEX_LEAF_HEADER_PAGE = 1;
constexpr page_id_t INDEX_INITIAL_ROOT_PAGE = 2;
constexpr int INDEX_INITIAL_PAGE_COUNT = 3;
constexpr int INDEX_MAX_KEY_LENGTH = 512;

/** @brief B+ 树节点页开头的固定布局。 */
struct IndexPageHeader {
    page_id_t next_free_page;      ///< 空闲页链表指针，当前实验暂未使用。
    page_id_t parent_page;         ///< 父节点页号；根节点为 INDEX_NO_PAGE。
    int key_count;                 ///< 当前节点中的键值对数量。
    bool is_leaf;                  ///< 是否为叶节点。
    page_id_t previous_leaf_page;  ///< 前一叶节点页号，仅叶节点有效。
    page_id_t next_leaf_page;      ///< 后一叶节点页号，仅叶节点有效。
};

static_assert(sizeof(IndexPageHeader) % alignof(Rid) == 0, "IndexPageHeader must preserve Rid alignment");

/** @brief 索引文件头在内存中的表示，并负责稳定的磁盘序列化。 */
class IndexFileHeader {
public:
    page_id_t first_free_page_{INDEX_NO_PAGE};  ///< 第一个空闲页页号。
    int page_count_{};                          ///< 已分配页面数量。
    page_id_t root_page_{INDEX_NO_PAGE};        ///< 根节点页号。
    int column_count_{};                        ///< 复合索引包含的列数。
    std::vector<ColType> column_types_;         ///< 各列类型，顺序即复合键顺序。
    std::vector<int> column_lengths_;           ///< 各列定长字节数。
    int key_length_{};                          ///< 一个完整复合键的字节数。
    int tree_order_{};                          ///< 单个节点可容纳的最大键值对数。
    int keys_region_size_{};                    ///< 对齐后的键数组区域字节数。
    page_id_t first_leaf_{INDEX_NO_PAGE};       ///< 第一个叶节点页号。
    page_id_t last_leaf_{INDEX_NO_PAGE};        ///< 最后一个叶节点页号。
    bool unique_{false};                        ///< 是否为唯一索引；默认允许重复键。
    int serialized_size_{};                     ///< 文件头序列化后的字节数。

    static constexpr size_t fixed_serialized_size() { return sizeof(page_id_t) * 4 + sizeof(int) * 7; }

    static constexpr size_t max_serialized_columns() {
        return (PAGE_SIZE - fixed_serialized_size()) / (sizeof(ColType) + sizeof(int));
    }

    static constexpr size_t serialized_size(size_t column_count) {
        return fixed_serialized_size() + (sizeof(ColType) + sizeof(int)) * column_count;
    }

    static int calculate_keys_region_size(int tree_order, int key_length);

    static bool page_layout_fits(int tree_order, int key_length);

    IndexFileHeader() = default;

    IndexFileHeader(page_id_t first_free_page,
                    int page_count,
                    page_id_t root_page,
                    int column_count,
                    int key_length,
                    int tree_order,
                    int keys_region_size,
                    page_id_t first_leaf,
                    page_id_t last_leaf,
                    bool unique = false);

    void update_serialized_size();

    void serialize(char* dest) const;

    void deserialize(const char* src, size_t src_len = PAGE_SIZE);
};

/** @brief 叶子页号与槽号组成的扫描位置。 */
struct IndexPosition {
    page_id_t page_no{INDEX_NO_PAGE};
    int slot_no{-1};

    friend bool operator==(const IndexPosition& left, const IndexPosition& right) noexcept {
        return left.page_no == right.page_no && left.slot_no == right.slot_no;
    }

    friend bool operator!=(const IndexPosition& left, const IndexPosition& right) noexcept { return !(left == right); }
};
