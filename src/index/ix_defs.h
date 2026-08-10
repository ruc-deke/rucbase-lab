// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

#include "common/defs.h"
#include "storage/buffer_pool_manager.h"

constexpr int IX_NO_PAGE = -1;
constexpr int IX_FILE_HDR_PAGE = 0;
constexpr int IX_LEAF_HEADER_PAGE = 1;
constexpr int IX_INIT_ROOT_PAGE = 2;
constexpr int IX_INIT_NUM_PAGES = 3;
constexpr int IX_MAX_COL_LEN = 512;

class IxPageHdr {
public:
    page_id_t next_free_page_no;  // unused
    page_id_t parent;             // 父亲节点所在页面的叶号
    int num_key;                  // # current keys (always equals to #child - 1) 已插入的keys数量
    bool is_leaf;                 // 是否为叶节点
    page_id_t prev_leaf;          // previous leaf node's page_no, effective only when is_leaf is true
    page_id_t next_leaf;          // next leaf node's page_no, effective only when is_leaf is true
};

static_assert(sizeof(IxPageHdr) % alignof(Rid) == 0, "IxPageHdr must preserve Rid alignment");

class IxFileHdr {
public:
    page_id_t first_free_page_no_{IX_NO_PAGE};  // 文件中第一个空闲的磁盘页面的页面号
    int num_pages_{};                           // 磁盘文件中页面的数量
    page_id_t root_page_{IX_NO_PAGE};           // B+树根节点对应的页面号
    int col_num_{};                             // 索引包含的字段数量
    std::vector<ColType> col_types_;            // 字段的类型
    std::vector<int> col_lens_;                 // 字段的长度
    int col_tot_len_{};                         // 索引包含的字段的总长度
    int btree_order_{};                         // # children per page 每个结点最多可插入的键值对数量
    int keys_size_{};                           // aligned byte size reserved for all key slots
    // first_leaf初始化之后没有进行修改，只不过是在测试文件中遍历叶子结点的时候用了
    page_id_t first_leaf_{IX_NO_PAGE};  // 首叶节点对应的页号，在上层IxManager的open函数进行初始化，初始化为root page_no
    page_id_t last_leaf_{IX_NO_PAGE};   // 尾叶节点对应的页号
    int tot_len_{};                     // 记录结构体的整体长度

    static constexpr size_t fixed_serialized_size() { return sizeof(page_id_t) * 4 + sizeof(int) * 6; }

    static constexpr size_t max_serialized_col_num() {
        return (PAGE_SIZE - fixed_serialized_size()) / (sizeof(ColType) + sizeof(int));
    }

    static constexpr size_t serialized_size(size_t col_num) {
        return fixed_serialized_size() + (sizeof(ColType) + sizeof(int)) * col_num;
    }

    static int calculate_keys_size(int btree_order, int col_tot_len) {
        if (btree_order < 0 || col_tot_len <= 0) {
            throw InternalError("Invalid index page layout");
        }
        int64_t raw_size = (static_cast<int64_t>(btree_order) + 1) * col_tot_len;
        int64_t alignment = alignof(Rid);
        int64_t aligned_size = (raw_size + alignment - 1) / alignment * alignment;
        if (aligned_size > std::numeric_limits<int>::max()) {
            throw InternalError("Invalid index page layout");
        }
        return static_cast<int>(aligned_size);
    }

    static bool page_layout_fits(int btree_order, int col_tot_len) {
        if (btree_order < 0 || col_tot_len <= 0) {
            return false;
        }
        int64_t slot_count = static_cast<int64_t>(btree_order) + 1;
        int64_t keys_size = calculate_keys_size(btree_order, col_tot_len);
        return sizeof(IxPageHdr) + keys_size + slot_count * sizeof(Rid) <= PAGE_SIZE;
    }

    IxFileHdr() = default;

    IxFileHdr(page_id_t first_free_page_no,
              int num_pages,
              page_id_t root_page,
              int col_num,
              int col_tot_len,
              int btree_order,
              int keys_size,
              page_id_t first_leaf,
              page_id_t last_leaf)
        : first_free_page_no_(first_free_page_no),
          num_pages_(num_pages),
          root_page_(root_page),
          col_num_(col_num),
          col_tot_len_(col_tot_len),
          btree_order_(btree_order),
          keys_size_(keys_size),
          first_leaf_(first_leaf),
          last_leaf_(last_leaf) {
        tot_len_ = 0;
    }

    void update_tot_len() {
        if (col_num_ <= 0 || static_cast<size_t>(col_num_) > max_serialized_col_num()) {
            throw InternalError("Index file header exceeds one page");
        }
        tot_len_ = static_cast<int>(serialized_size(col_num_));
    }

    void serialize(char* dest) const {
        if (dest == nullptr || col_num_ <= 0 || static_cast<size_t>(col_num_) > max_serialized_col_num() ||
            col_types_.size() != static_cast<size_t>(col_num_) || col_lens_.size() != static_cast<size_t>(col_num_) ||
            tot_len_ != static_cast<int>(serialized_size(col_num_))) {
            throw InternalError("Invalid index file header");
        }
        int offset = 0;
        memcpy(dest + offset, &tot_len_, sizeof(int));
        offset += sizeof(int);
        memcpy(dest + offset, &first_free_page_no_, sizeof(page_id_t));
        offset += sizeof(page_id_t);
        memcpy(dest + offset, &num_pages_, sizeof(int));
        offset += sizeof(int);
        memcpy(dest + offset, &root_page_, sizeof(page_id_t));
        offset += sizeof(page_id_t);
        memcpy(dest + offset, &col_num_, sizeof(int));
        offset += sizeof(int);
        for (int i = 0; i < col_num_; ++i) {
            memcpy(dest + offset, &col_types_[i], sizeof(ColType));
            offset += sizeof(ColType);
        }
        for (int i = 0; i < col_num_; ++i) {
            memcpy(dest + offset, &col_lens_[i], sizeof(int));
            offset += sizeof(int);
        }
        memcpy(dest + offset, &col_tot_len_, sizeof(int));
        offset += sizeof(int);
        memcpy(dest + offset, &btree_order_, sizeof(int));
        offset += sizeof(int);
        memcpy(dest + offset, &keys_size_, sizeof(int));
        offset += sizeof(int);
        memcpy(dest + offset, &first_leaf_, sizeof(page_id_t));
        offset += sizeof(page_id_t);
        memcpy(dest + offset, &last_leaf_, sizeof(page_id_t));
        offset += sizeof(page_id_t);
        if (offset != tot_len_) {
            throw InternalError("Invalid index file header");
        }
    }

    void deserialize(const char* src, size_t src_len = PAGE_SIZE) {
        if (src == nullptr || src_len < fixed_serialized_size()) {
            throw InternalError("Invalid index file header");
        }

        size_t offset = 0;
        auto read_value = [&](auto& value) {
            memcpy(&value, src + offset, sizeof(value));
            offset += sizeof(value);
        };

        read_value(tot_len_);
        read_value(first_free_page_no_);
        read_value(num_pages_);
        read_value(root_page_);
        read_value(col_num_);
        if (col_num_ <= 0 || static_cast<size_t>(col_num_) > max_serialized_col_num() ||
            serialized_size(col_num_) != static_cast<size_t>(tot_len_) || static_cast<size_t>(tot_len_) > src_len) {
            throw InternalError("Invalid index file header");
        }

        col_types_.resize(col_num_);
        col_lens_.resize(col_num_);
        for (auto& type : col_types_) {
            read_value(type);
        }
        int64_t col_len_sum = 0;
        for (auto& len : col_lens_) {
            read_value(len);
            if (len <= 0) {
                throw InternalError("Invalid index file header");
            }
            col_len_sum += len;
        }
        read_value(col_tot_len_);
        read_value(btree_order_);
        read_value(keys_size_);
        read_value(first_leaf_);
        read_value(last_leaf_);

        if (col_len_sum != col_tot_len_ || col_tot_len_ <= 0 || col_tot_len_ > IX_MAX_COL_LEN || btree_order_ <= 2 ||
            keys_size_ != calculate_keys_size(btree_order_, col_tot_len_) ||
            !page_layout_fits(btree_order_, col_tot_len_)) {
            throw InternalError("Invalid index file header");
        }
    }
};

class Iid {
public:
    int page_no;
    int slot_no;

    friend bool operator==(const Iid& x, const Iid& y) { return x.page_no == y.page_no && x.slot_no == y.slot_no; }

    friend bool operator!=(const Iid& x, const Iid& y) { return !(x == y); }
};
