#pragma once

#include <memory>
#include <string>

#include "ix_defs.h"
#include "ix_index_handle.h"

class IxManager {
   private:
    DiskManager *disk_manager_;
    BufferPoolManager *buffer_pool_manager_;

   public:
    IxManager(DiskManager *disk_manager, BufferPoolManager *buffer_pool_manager)
        : disk_manager_(disk_manager), buffer_pool_manager_(buffer_pool_manager) {}

    std::string get_index_name(const std::string &filename, int index_no) {
        return filename + '.' + std::to_string(index_no) + ".idx";
    }

    bool exists(const std::string &filename, int index_no) {
        auto ix_name = get_index_name(filename, index_no);
        return disk_manager_->is_file(ix_name);
    }

    void create_index(const std::string &filename, int index_no, ColType col_type, int col_len) {
        std::string ix_name = get_index_name(filename, index_no);
        assert(index_no >= 0);
        // Create index file
        disk_manager_->create_file(ix_name);
        // Open index file
        int fd = disk_manager_->open_file(ix_name);
        // Create file header and write to file
        // Theoretically we have: |page_hdr| + (|attr| + |rid|) * n <= PAGE_SIZE
        // but we reserve one slot for convenient inserting and deleting, i.e.
        // |page_hdr| + (|attr| + |rid|) * (n + 1) <= PAGE_SIZE
        if (col_len > IX_MAX_COL_LEN) {
            throw InvalidColLengthError(col_len);
        }
        // 根据 |page_hdr| + (|attr| + |rid|) * (n + 1) <= PAGE_SIZE 求得n的最大值btree_order
        // 即 n <= btree_order，那么btree_order就是每个结点最多可插入的键值对数量（实际还多留了一个空位，但其不可插入）
        int btree_order = static_cast<int>((PAGE_SIZE - sizeof(IxPageHdr)) / (col_len + sizeof(Rid)) - 1);
        assert(btree_order > 2);
        // int key_offset = sizeof(IxPageHdr);
        // int rid_offset = key_offset + (btree_order + 1) * col_len;

        // Create file header and write to file
        IxFileHdr fhdr = {
            .first_free_page_no = IX_NO_PAGE,
            .num_pages = IX_INIT_NUM_PAGES,
            .root_page = IX_INIT_ROOT_PAGE,
            .col_type = col_type,
            .col_len = col_len,
            .btree_order = btree_order,
            // .key_offset = key_offset,
            // .rid_offset = rid_offset,
            .keys_size = (btree_order + 1) * col_len,  // 用于IxNodeHandle初始化rids首地址
            .first_leaf = IX_INIT_ROOT_PAGE,
            .last_leaf = IX_INIT_ROOT_PAGE,
        };
        disk_manager_->write_page(fd, IX_FILE_HDR_PAGE, (const char *)&fhdr, sizeof(fhdr));

        char page_buf[PAGE_SIZE];  // 在内存中初始化page_buf中的内容，然后将其写入磁盘
        // 注意leaf header页号为1，也标记为叶子结点，其前一个/后一个叶子均指向root node
        // Create leaf list header page and write to file
        {
            auto phdr = reinterpret_cast<IxPageHdr *>(page_buf);
            *phdr = {
                .next_free_page_no = IX_NO_PAGE,
                .parent = IX_NO_PAGE,
                .num_key = 0,
                .is_leaf = true,
                .prev_leaf = IX_INIT_ROOT_PAGE,
                .next_leaf = IX_INIT_ROOT_PAGE,
            };
            disk_manager_->write_page(fd, IX_LEAF_HEADER_PAGE, page_buf, PAGE_SIZE);
        }
        // 注意root node页号为2，也标记为叶子结点，其前一个/后一个叶子均指向leaf header
        // Create root node and write to file
        {
            auto phdr = reinterpret_cast<IxPageHdr *>(page_buf);
            *phdr = {
                .next_free_page_no = IX_NO_PAGE,
                .parent = IX_NO_PAGE,
                .num_key = 0,
                .is_leaf = true,
                .prev_leaf = IX_LEAF_HEADER_PAGE,
                .next_leaf = IX_LEAF_HEADER_PAGE,
            };
            // Must write PAGE_SIZE here in case of future fetch_node()
            disk_manager_->write_page(fd, IX_INIT_ROOT_PAGE, page_buf, PAGE_SIZE);
        }

        disk_manager_->set_fd2pageno(fd, IX_INIT_NUM_PAGES - 1);  // DEBUG

        // Close index file
        disk_manager_->close_file(fd);
    }

    void destroy_index(const std::string &filename, int index_no) {
        std::string ix_name = get_index_name(filename, index_no);
        disk_manager_->destroy_file(ix_name);
    }

    // 注意这里打开文件，创建并返回了index file handle的指针
    std::unique_ptr<IxIndexHandle> open_index(const std::string &filename, int index_no) {
        std::string ix_name = get_index_name(filename, index_no);
        int fd = disk_manager_->open_file(ix_name);
        return std::make_unique<IxIndexHandle>(disk_manager_, buffer_pool_manager_, fd);
    }

    void close_index(const IxIndexHandle *ih) {
        disk_manager_->write_page(ih->fd_, IX_FILE_HDR_PAGE, (const char *)&ih->file_hdr_, sizeof(ih->file_hdr_));
        // 缓冲区的所有页刷到磁盘，注意这句话必须写在close_file前面
        buffer_pool_manager_->FlushAllPages(ih->fd_);
        disk_manager_->close_file(ih->fd_);
    }
};
