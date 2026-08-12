// Copyright (c) 2023-2026 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#pragma once

#include <cassert>
#include <cstddef>
#include <memory>

#include "bitmap.h"
#include "common/defs.h"
#include "common/errors.h"
#include "rm_defs.h"
#include "storage/buffer_pool_manager.h"
#include "storage/disk_manager.h"
#include "storage/page.h"

class Context;
class RmManager;

/**
 * @brief 将一张已固定在缓冲池中的页面解释为记录页布局。
 *
 * 本类型只是页面视图，不拥有 Page，也不负责解除 pin。持有它的作用域必须保证页面保持固定；
 * 获取页面的一方仍负责恰好解除一次 pin。
 */
struct RmPageHandle {
    const RmFileHdr* file_hdr;  // 当前页面所在文件的文件头指针
    Page* page;                 // 页面的实际数据，包括页面存储的数据、元信息等
    RmPageHdr* page_hdr;        // page->data的第一部分，存储页面元信息，指针指向首地址，长度为sizeof(RmPageHdr)
    char* bitmap;               // page->data的第二部分，存储页面的bitmap，指针指向首地址，长度为file_hdr->bitmap_size
    char* slots;  // page->data的第三部分，存储表的记录，指针指向首地址，每个slot的长度为file_hdr->record_size

    /** @throws InternalError 文件头或页面为空。 */
    RmPageHandle(const RmFileHdr* fhdr_, Page* page_) : file_hdr(fhdr_), page(page_) {
        if (file_hdr == nullptr || page == nullptr) {
            throw InternalError("cannot construct a record page handle from null");
        }
        // Page 的开头预留给通用页面信息（目前是 LSN），记录页头从 OFFSET_PAGE_HDR 开始。
        page_hdr = reinterpret_cast<RmPageHdr*>(page->get_data() + Page::OFFSET_PAGE_HDR);

        // bitmap 紧跟在记录页头后面，每一位表示一个 slot 是否存有记录。
        bitmap = page->get_data() + Page::OFFSET_PAGE_HDR + sizeof(RmPageHdr);

        // 定长记录槽紧跟在 bitmap 后面，可用“起始地址 + slot_no * 记录长度”定位记录。
        slots = bitmap + file_hdr->bitmap_size;
    }

    /** @return slot_no 对应记录槽的首地址。 */
    char* get_slot(int slot_no) const { return slots + static_cast<std::ptrdiff_t>(slot_no) * file_hdr->record_size; }
};

/* 每个RmFileHandle对应一个表的数据文件，里面有多个page，每个page的数据封装在RmPageHandle中 */
class RmFileHandle {
    friend class RmScan;
    friend class RmManager;

private:
    DiskManager* disk_manager_;
    BufferPoolManager* buffer_pool_manager_;
    int fd_;              // 打开文件后产生的文件句柄
    RmFileHdr file_hdr_;  // 文件头，维护当前表文件的元数据

public:
    RmFileHandle(DiskManager* disk_manager, BufferPoolManager* buffer_pool_manager, int fd)
        : disk_manager_(disk_manager),
          buffer_pool_manager_(buffer_pool_manager),
          fd_(fd) {
        // 注意：这里从磁盘中读出文件描述符为fd的文件的file_hdr，读到内存中
        // 这里实际就是初始化file_hdr，只不过是从磁盘中读出进行初始化
        // init file_hdr_
        disk_manager_->read_page(fd, RM_FILE_HDR_PAGE, reinterpret_cast<char*>(&file_hdr_), sizeof(file_hdr_));
        // disk_manager管理的fd对应的文件中，设置从file_hdr_.num_pages开始分配page_no
        disk_manager_->set_fd2pageno(fd, file_hdr_.num_pages);
    }

    RmFileHdr get_file_hdr() const { return file_hdr_; }
    int GetFd() const noexcept { return fd_; }

    /** @brief 通过 bitmap 判断指定位置是否已经存在记录。 */
    bool is_record(const Rid& rid) const {
        RmPageHandle page_handle = fetch_page_handle(rid.page_no);
        bool exists = Bitmap::is_set(page_handle.bitmap, rid.slot_no);
        bool unpinned = buffer_pool_manager_->unpin_page(page_handle.page->get_page_id(), false);
        assert(unpinned);
        (void)unpinned;
        return exists;
    }

    std::unique_ptr<RmRecord> get_record(const Rid& rid, Context* context) const;

    Rid insert_record(const char* buf, Context* context);

    /** @brief 在恢复或回滚时把记录恢复到指定 RID；不属于 Lab1 评分接口。 */
    void insert_record(const Rid& rid, const char* buf);

    void delete_record(const Rid& rid, Context* context);

    void update_record(const Rid& rid, const char* buf, Context* context);

    RmPageHandle create_new_page_handle();

    RmPageHandle fetch_page_handle(int page_no) const;

private:
    RmPageHandle create_page_handle();

    void release_page_handle(RmPageHandle& page_handle);
};
