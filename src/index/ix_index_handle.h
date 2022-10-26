#pragma once

#include "ix_defs.h"
#include "ix_node_handle.h"
#include "transaction/transaction.h"

enum class Operation { FIND = 0, INSERT, DELETE };  // 三种操作：查找、插入、删除

/**
 * @brief B+树索引
 */
class IxIndexHandle {
    friend class IxScan;
    friend class IxManager;

   private:
    DiskManager *disk_manager_;
    BufferPoolManager *buffer_pool_manager_;
    int fd_;
    IxFileHdr file_hdr_;  // 存了root_page，但root_page初始化为2（第0页存FILE_HDR_PAGE，第1页存LEAF_HEADER_PAGE）
    std::mutex root_latch_;  // 用于索引并发（请自行选择并发粒度在 Tree级 或 Page级 ）

   public:
    IxIndexHandle(DiskManager *disk_manager, BufferPoolManager *buffer_pool_manager, int fd);

    // for search
    bool GetValue(const char *key, std::vector<Rid> *result, Transaction *transaction);

    IxNodeHandle *FindLeafPage(const char *key, Operation operation, Transaction *transaction);

    // for insert
    bool insert_entry(const char *key, const Rid &value, Transaction *transaction);

    IxNodeHandle *Split(IxNodeHandle *node);

    void InsertIntoParent(IxNodeHandle *old_node, const char *key, IxNodeHandle *new_node, Transaction *transaction);

    // for delete
    bool delete_entry(const char *key, Transaction *transaction);

    bool CoalesceOrRedistribute(IxNodeHandle *node, Transaction *transaction = nullptr);

    bool AdjustRoot(IxNodeHandle *old_root_node);

    void Redistribute(IxNodeHandle *neighbor_node, IxNodeHandle *node, IxNodeHandle *parent, int index);

    bool Coalesce(IxNodeHandle **neighbor_node, IxNodeHandle **node, IxNodeHandle **parent, int index,
                  Transaction *transaction);

    // 辅助函数，lab3执行层将使用
    Iid lower_bound(const char *key);

    Iid upper_bound(const char *key);

    Iid leaf_end() const;

    Iid leaf_begin() const;

   private:
    // 辅助函数
    void UpdateRootPageNo(page_id_t root) { file_hdr_.root_page = root; }

    bool IsEmpty() const { return file_hdr_.root_page == IX_NO_PAGE; }

    // for get/create node
    IxNodeHandle *FetchNode(int page_no) const;

    IxNodeHandle *CreateNode();

    // for maintain data structure
    void maintain_parent(IxNodeHandle *node);

    void erase_leaf(IxNodeHandle *leaf);

    void release_node_handle(IxNodeHandle &node);

    void maintain_child(IxNodeHandle *node, int child_idx);

    // for index test
    Rid get_rid(const Iid &iid) const;
};
