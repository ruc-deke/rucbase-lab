#pragma once

#include <assert.h>

#include "bitmap.h"
#include "rm_defs.h"
#include "rm_file_handle.h"

//只用于创建/打开/关闭/删除文件，打开文件的时候会返回record file handle
//它可以管理多个record文件（管理多个record file handle）
class RmManager {
   private:
    DiskManager *disk_manager_;
    BufferPoolManager *buffer_pool_manager_;

   public:
    RmManager(DiskManager *disk_manager, BufferPoolManager *buffer_pool_manager)
        : disk_manager_(disk_manager), buffer_pool_manager_(buffer_pool_manager) {}

    void create_file(const std::string &filename, int record_size) {
        if (record_size < 1 || record_size > RM_MAX_RECORD_SIZE) {
            throw InvalidRecordSizeError(record_size);
        }
        disk_manager_->create_file(filename);
        int fd = disk_manager_->open_file(filename);

        // 初始化file header
        RmFileHdr file_hdr{};
        file_hdr.record_size = record_size;
        file_hdr.num_pages = 1;
        file_hdr.first_free_page_no = RM_NO_PAGE;
        // We have: sizeof(hdr) + (n + 7) / 8 + n * record_size <= PAGE_SIZE
        file_hdr.num_records_per_page =
            (BITMAP_WIDTH * (PAGE_SIZE - 1 - (int)sizeof(RmFileHdr)) + 1) / (1 + record_size * BITMAP_WIDTH);
        file_hdr.bitmap_size = (file_hdr.num_records_per_page + BITMAP_WIDTH - 1) / BITMAP_WIDTH;

        // 将file header写入磁盘文件（名为file name，文件描述符为fd）中的第0页
        // head page直接写入磁盘，没有经过缓冲区的NewPage，那么也就不需要FlushPage
        disk_manager_->write_page(fd, RM_FILE_HDR_PAGE, (char *)&file_hdr, sizeof(file_hdr));
        disk_manager_->close_file(fd);
    }

    void destroy_file(const std::string &filename) { disk_manager_->destroy_file(filename); }

    // 注意这里打开文件，创建并返回了record file handle的指针
    std::unique_ptr<RmFileHandle> open_file(const std::string &filename) {
        int fd = disk_manager_->open_file(filename);
        return std::make_unique<RmFileHandle>(disk_manager_, buffer_pool_manager_, fd);
    }

    void close_file(const RmFileHandle *file_handle) {
        disk_manager_->write_page(file_handle->fd_, RM_FILE_HDR_PAGE, (char *)&file_handle->file_hdr_,
                                  sizeof(file_handle->file_hdr_));
        // 缓冲区的所有页刷到磁盘，注意这句话必须写在close_file前面
        buffer_pool_manager_->FlushAllPages(file_handle->fd_);
        disk_manager_->close_file(file_handle->fd_);
    }
};
