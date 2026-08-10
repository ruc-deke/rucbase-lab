// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <memory>
#include <string>

#include "system/sm_meta.h"
#include "ix_defs.h"
#include "ix_index_handle.h"

class IxManager {
   private:
    DiskManager *disk_manager_;
    BufferPoolManager *buffer_pool_manager_;

   public:
    IxManager(DiskManager *disk_manager, BufferPoolManager *buffer_pool_manager)
        : disk_manager_(disk_manager), buffer_pool_manager_(buffer_pool_manager) {}

    std::string get_index_name(const std::string &filename, const std::vector<std::string>& index_cols) {
        std::string index_name = filename;
        // 范围 for：按列名拼接索引文件名，等价于下标循环 for (size_t i = 0; i < n; ++i)。
        for (const auto& col : index_cols) {
            index_name += "_" + col;
        }
        index_name += ".idx";

        return index_name;
    }

    std::string get_index_name(const std::string &filename, const std::vector<ColMeta>& index_cols) {
        std::string index_name = filename;
        for (const auto& col : index_cols) {
            index_name += "_" + col.name;
        }
        index_name += ".idx";

        return index_name;
    }

    bool exists(const std::string &filename, const std::vector<ColMeta>& index_cols) {
        auto ix_name = get_index_name(filename, index_cols);
        return disk_manager_->is_file(ix_name);
    }

    bool exists(const std::string &filename, const std::vector<std::string>& index_cols) {
        auto ix_name = get_index_name(filename, index_cols);
        return disk_manager_->is_file(ix_name);
    }

    void create_index(const std::string &filename, const std::vector<ColMeta>& index_cols) {
        // Theoretically we have: |page_hdr| + (|attr| + |rid|) * n <= PAGE_SIZE
        // but we reserve one slot for convenient inserting and deleting, i.e.
        // |page_hdr| + (|attr| + |rid|) * (n + 1) <= PAGE_SIZE
        if (index_cols.empty() || index_cols.size() > IxFileHdr::max_serialized_col_num()) {
            throw InternalError("Invalid number of index columns");
        }

        int64_t checked_col_tot_len = 0;
        for (const auto &col : index_cols) {
            bool valid_len = col.len > 0;
            switch (col.type) {
                case TYPE_INT:
                    valid_len = valid_len && col.len == static_cast<int>(sizeof(int));
                    break;
                case TYPE_FLOAT:
                    valid_len = valid_len && col.len == static_cast<int>(sizeof(float));
                    break;
                case TYPE_STRING:
                    break;
                default:
                    throw InternalError("Unexpected data type");
            }
            if (!valid_len) {
                throw InvalidColLengthError(col.len);
            }
            checked_col_tot_len += col.len;
            if (checked_col_tot_len > IX_MAX_COL_LEN) {
                throw InvalidColLengthError(static_cast<int>(checked_col_tot_len));
            }
        }
        int col_num = static_cast<int>(index_cols.size());
        int col_tot_len = static_cast<int>(checked_col_tot_len);

        // 根据 |page_hdr| + (|attr| + |rid|) * (n + 1) <= PAGE_SIZE 求得n的最大值btree_order
        // 即 n <= btree_order，那么btree_order就是每个结点最多可插入的键值对数量（实际还多留了一个空位，但其不可插入）
        int btree_order = static_cast<int>((PAGE_SIZE - sizeof(IxPageHdr)) / (col_tot_len + sizeof(Rid)) - 1);
        while (btree_order > 2 && !IxFileHdr::page_layout_fits(btree_order, col_tot_len)) {
            --btree_order;
        }
        if (btree_order <= 2 || !IxFileHdr::page_layout_fits(btree_order, col_tot_len)) {
            throw InvalidColLengthError(col_tot_len);
        }
        int keys_size = IxFileHdr::calculate_keys_size(btree_order, col_tot_len);

        IxFileHdr fhdr(IX_NO_PAGE, IX_INIT_NUM_PAGES, IX_INIT_ROOT_PAGE,
                                col_num, col_tot_len, btree_order, keys_size,
                                IX_INIT_ROOT_PAGE, IX_INIT_ROOT_PAGE);
        for(int i = 0; i < col_num; ++i) {
            fhdr.col_types_.push_back(index_cols[i].type);
            fhdr.col_lens_.push_back(index_cols[i].len);
        }
        fhdr.update_tot_len();
        
        std::vector<char> data(fhdr.tot_len_);
        fhdr.serialize(data.data());

        // Do not leave an index file behind for invalid metadata.
        std::string ix_name = get_index_name(filename, index_cols);
        disk_manager_->create_file(ix_name);
        int fd = disk_manager_->open_file(ix_name);

        disk_manager_->write_page(fd, IX_FILE_HDR_PAGE, data.data(), fhdr.tot_len_);

        alignas(IxPageHdr) char page_buf[PAGE_SIZE];  // 在内存中初始化page_buf中的内容，然后将其写入磁盘
        memset(page_buf, 0, PAGE_SIZE);
        // 注意leaf header页号为1，也标记为叶子结点，其前一个/后一个叶子均指向root node
        // Create leaf list header page and write to file
        {
            memset(page_buf, 0, PAGE_SIZE);
            auto phdr = reinterpret_cast<IxPageHdr *>(page_buf);
            phdr->next_free_page_no = IX_NO_PAGE;
            phdr->parent = IX_NO_PAGE;
            phdr->num_key = 0;
            phdr->is_leaf = true;
            phdr->prev_leaf = IX_INIT_ROOT_PAGE;
            phdr->next_leaf = IX_INIT_ROOT_PAGE;
            disk_manager_->write_page(fd, IX_LEAF_HEADER_PAGE, page_buf, PAGE_SIZE);
        }
        // 注意root node页号为2，也标记为叶子结点，其前一个/后一个叶子均指向leaf header
        // Create root node and write to file
        {
            memset(page_buf, 0, PAGE_SIZE);
            auto phdr = reinterpret_cast<IxPageHdr *>(page_buf);
            phdr->next_free_page_no = IX_NO_PAGE;
            phdr->parent = IX_NO_PAGE;
            phdr->num_key = 0;
            phdr->is_leaf = true;
            phdr->prev_leaf = IX_LEAF_HEADER_PAGE;
            phdr->next_leaf = IX_LEAF_HEADER_PAGE;
            // Must write PAGE_SIZE here in case of future fetch_node()
            disk_manager_->write_page(fd, IX_INIT_ROOT_PAGE, page_buf, PAGE_SIZE);
        }

        disk_manager_->set_fd2pageno(fd, IX_INIT_NUM_PAGES);

        // Close index file
        disk_manager_->close_file(fd);
    }

    void destroy_index(const std::string &filename, const std::vector<ColMeta>& index_cols) {
        std::string ix_name = get_index_name(filename, index_cols);
        disk_manager_->destroy_file(ix_name);
    }

    void destroy_index(const std::string &filename, const std::vector<std::string>& index_cols) {
        std::string ix_name = get_index_name(filename, index_cols);
        disk_manager_->destroy_file(ix_name);
    }

    // 注意这里打开文件，创建并返回了index file handle的指针
    std::unique_ptr<IxIndexHandle> open_index(const std::string &filename, const std::vector<ColMeta>& index_cols) {
        std::string ix_name = get_index_name(filename, index_cols);
        int fd = disk_manager_->open_file(ix_name);
        try {
            return std::make_unique<IxIndexHandle>(disk_manager_, buffer_pool_manager_, fd);
        } catch (...) {
            disk_manager_->close_file(fd);
            throw;
        }
    }

    std::unique_ptr<IxIndexHandle> open_index(const std::string &filename, const std::vector<std::string>& index_cols) {
        std::string ix_name = get_index_name(filename, index_cols);
        int fd = disk_manager_->open_file(ix_name);
        try {
            return std::make_unique<IxIndexHandle>(disk_manager_, buffer_pool_manager_, fd);
        } catch (...) {
            disk_manager_->close_file(fd);
            throw;
        }
    }

    void close_index(const IxIndexHandle *ih) {
        std::vector<char> data(ih->file_hdr_->tot_len_);
        ih->file_hdr_->serialize(data.data());
        disk_manager_->write_page(ih->fd_, IX_FILE_HDR_PAGE, data.data(), ih->file_hdr_->tot_len_);
        // 缓冲区的所有页刷到磁盘，注意这句话必须写在close_file前面
        buffer_pool_manager_->flush_all_pages(ih->fd_);
        disk_manager_->close_file(ih->fd_);
    }
};
