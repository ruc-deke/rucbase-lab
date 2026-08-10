// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "index_types.h"

#include <cstdint>
#include <cstring>
#include <limits>

#include "common/errors.h"

int IndexFileHeader::calculate_keys_region_size(int tree_order, int key_length) {
    if (tree_order < 0 || key_length <= 0) {
        throw InternalError("Invalid index page layout");
    }
    int64_t raw_size = (static_cast<int64_t>(tree_order) + 1) * key_length;
    int64_t alignment = alignof(Rid);
    int64_t aligned_size = (raw_size + alignment - 1) / alignment * alignment;
    if (aligned_size > std::numeric_limits<int>::max()) {
        throw InternalError("Invalid index page layout");
    }
    return static_cast<int>(aligned_size);
}

bool IndexFileHeader::page_layout_fits(int tree_order, int key_length) {
    if (tree_order < 0 || key_length <= 0) {
        return false;
    }
    int64_t slot_count = static_cast<int64_t>(tree_order) + 1;
    int64_t keys_region_size = calculate_keys_region_size(tree_order, key_length);
    return sizeof(IndexPageHeader) + keys_region_size + slot_count * sizeof(Rid) <= PAGE_SIZE;
}

IndexFileHeader::IndexFileHeader(page_id_t first_free_page,
                                 int page_count,
                                 page_id_t root_page,
                                 int column_count,
                                 int key_length,
                                 int tree_order,
                                 int keys_region_size,
                                 page_id_t first_leaf,
                                 page_id_t last_leaf)
    : first_free_page_(first_free_page),
      page_count_(page_count),
      root_page_(root_page),
      column_count_(column_count),
      key_length_(key_length),
      tree_order_(tree_order),
      keys_region_size_(keys_region_size),
      first_leaf_(first_leaf),
      last_leaf_(last_leaf) {
    serialized_size_ = 0;
}

void IndexFileHeader::update_serialized_size() {
    if (column_count_ <= 0 || static_cast<size_t>(column_count_) > max_serialized_columns()) {
        throw InternalError("Index file header exceeds one page");
    }
    serialized_size_ = static_cast<int>(serialized_size(column_count_));
}

void IndexFileHeader::serialize(char* dest) const {
    if (dest == nullptr || column_count_ <= 0 || static_cast<size_t>(column_count_) > max_serialized_columns() ||
        column_types_.size() != static_cast<size_t>(column_count_) ||
        column_lengths_.size() != static_cast<size_t>(column_count_) ||
        serialized_size_ != static_cast<int>(serialized_size(column_count_))) {
        throw InternalError("Invalid index file header");
    }
    int offset = 0;
    memcpy(dest + offset, &serialized_size_, sizeof(int));
    offset += sizeof(int);
    memcpy(dest + offset, &first_free_page_, sizeof(page_id_t));
    offset += sizeof(page_id_t);
    memcpy(dest + offset, &page_count_, sizeof(int));
    offset += sizeof(int);
    memcpy(dest + offset, &root_page_, sizeof(page_id_t));
    offset += sizeof(page_id_t);
    memcpy(dest + offset, &column_count_, sizeof(int));
    offset += sizeof(int);
    for (int i = 0; i < column_count_; ++i) {
        memcpy(dest + offset, &column_types_[i], sizeof(ColType));
        offset += sizeof(ColType);
    }
    for (int i = 0; i < column_count_; ++i) {
        memcpy(dest + offset, &column_lengths_[i], sizeof(int));
        offset += sizeof(int);
    }
    memcpy(dest + offset, &key_length_, sizeof(int));
    offset += sizeof(int);
    memcpy(dest + offset, &tree_order_, sizeof(int));
    offset += sizeof(int);
    memcpy(dest + offset, &keys_region_size_, sizeof(int));
    offset += sizeof(int);
    memcpy(dest + offset, &first_leaf_, sizeof(page_id_t));
    offset += sizeof(page_id_t);
    memcpy(dest + offset, &last_leaf_, sizeof(page_id_t));
    offset += sizeof(page_id_t);
    if (offset != serialized_size_) {
        throw InternalError("Invalid index file header");
    }
}

void IndexFileHeader::deserialize(const char* src, size_t src_len) {
    if (src == nullptr || src_len < fixed_serialized_size()) {
        throw InternalError("Invalid index file header");
    }

    size_t offset = 0;
    auto read_value = [&](auto& value) {
        memcpy(&value, src + offset, sizeof(value));
        offset += sizeof(value);
    };

    read_value(serialized_size_);
    read_value(first_free_page_);
    read_value(page_count_);
    read_value(root_page_);
    read_value(column_count_);
    if (column_count_ <= 0 || static_cast<size_t>(column_count_) > max_serialized_columns() ||
        serialized_size(column_count_) != static_cast<size_t>(serialized_size_) ||
        static_cast<size_t>(serialized_size_) > src_len) {
        throw InternalError("Invalid index file header");
    }

    column_types_.resize(column_count_);
    column_lengths_.resize(column_count_);
    for (auto& type : column_types_) {
        read_value(type);
    }
    int64_t col_len_sum = 0;
    for (auto& len : column_lengths_) {
        read_value(len);
        if (len <= 0) {
            throw InternalError("Invalid index file header");
        }
        col_len_sum += len;
    }
    read_value(key_length_);
    read_value(tree_order_);
    read_value(keys_region_size_);
    read_value(first_leaf_);
    read_value(last_leaf_);

    if (col_len_sum != key_length_ || key_length_ <= 0 || key_length_ > INDEX_MAX_KEY_LENGTH || tree_order_ <= 2 ||
        keys_region_size_ != calculate_keys_region_size(tree_order_, key_length_) ||
        !page_layout_fits(tree_order_, key_length_)) {
        throw InternalError("Invalid index file header");
    }
}
