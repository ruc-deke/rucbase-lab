#pragma once

#include "ix_defs.h"
#include "transaction/transaction.h"

enum class Operation { FIND = 0, INSERT, DELETE };  // 三种操作：查找、插入、删除

static const bool binary_search = false;

inline int ix_compare(const char *a, const char *b, ColType type, int col_len) {
    switch (type) {
        case TYPE_INT: {
            int ia = *(int *)a;
            int ib = *(int *)b;
            return (ia < ib) ? -1 : ((ia > ib) ? 1 : 0);
        }
        case TYPE_FLOAT: {
            float fa = *(float *)a;
            float fb = *(float *)b;
            return (fa < fb) ? -1 : ((fa > fb) ? 1 : 0);
        }
        case TYPE_STRING:
            return memcmp(a, b, col_len);
        default:
            throw InternalError("Unexpected data type");
    }
}

// 可类比RmPageHandle
class IxNodeHandle {
    friend class IxIndexHandle;
    friend class IxScan;

   private:
    const IxFileHdr *file_hdr;  // 用到了file_hdr的keys_size, col_len
    Page *page;
    IxPageHdr *page_hdr;  // page->data的第一部分，指针指向首地址，长度为sizeof(IxPageHdr)
    char *keys;  // page->data的第二部分，指针指向首地址，长度为file_hdr->keys_size，每个key的长度为file_hdr->col_len
    Rid *rids;   // page->data的第三部分，指针指向首地址

   public:
    int GetSize() { return page_hdr->num_key; }

    void SetSize(int size) { page_hdr->num_key = size; }

    int GetMaxSize() { return file_hdr->btree_order + 1; }

    int GetMinSize() { return GetMaxSize() / 2; }

    int KeyAt(int i) { return *(int *)get_key(i); }

    /**
     * @brief 得到第i个孩子结点的page_no
     */
    page_id_t ValueAt(int i) { return get_rid(i)->page_no; }

    page_id_t GetPageNo() { return page->GetPageId().page_no; }

    PageId GetPageId() { return page->GetPageId(); }

    page_id_t GetNextLeaf() { return page_hdr->next_leaf; }

    page_id_t GetPrevLeaf() { return page_hdr->prev_leaf; }

    page_id_t GetParentPageNo() { return page_hdr->parent; }

    bool IsLeafPage() { return page_hdr->is_leaf; }

    bool IsRootPage() { return GetParentPageNo() == INVALID_PAGE_ID; }

    void SetNextLeaf(page_id_t page_no) { page_hdr->next_leaf = page_no; }

    void SetPrevLeaf(page_id_t page_no) { page_hdr->prev_leaf = page_no; }

    void SetParentPageNo(page_id_t parent) { page_hdr->parent = parent; }

    /**
     * @brief used in internal node to remove the last key in root node, and return the last child
     *
     * @return the last child
     */
    page_id_t RemoveAndReturnOnlyChild();

    /**
     * @brief used in internal node to find the page which store the target key
     *
     * @return the page's page_no which store the target key
     */
    page_id_t InternalLookup(const char *key);

    bool LeafLookup(const char *key, Rid **value);

    /**
     * @brief used in leaf node to insert (key,value)
     *
     * @return the size after Insert
     */
    int Insert(const char *key, const Rid &value);

    /**
     * @brief used in leaf node to remove (key,value)
     *
     * @return the size after Remove
     */
    int Remove(const char *key);

    IxNodeHandle() = default;

    IxNodeHandle(const IxFileHdr *file_hdr_, Page *page_);

    char *get_key(int key_idx) const;

    Rid *get_rid(int rid_idx) const;

    void set_key(int key_idx, const char *key);

    void set_rid(int rid_idx, const Rid &rid);

    /**
     * @brief 在当前node中查找第一个>=target的key_idx
     *
     * @return key_idx，范围为[0,num_key)，如果返回的key_idx=num_key，则表示target大于最后一个key
     * @note 返回key index（同时也是rid index），作为slot no
     */
    int lower_bound(const char *target) const;

    /**
     * @brief 在当前node中查找第一个>target的key_idx
     *
     * @return key_idx，范围为[0,num_key)，如果返回的key_idx=num_key，则表示target大于等于最后一个key
     */
    int upper_bound(const char *target) const;

    /**
     * @brief 将key的前n位插入到原来keys中的pos位置；将rid的前n位插入到原来rids中的pos位置
     *
     * @note [0,pos)           [pos,num_key)
     *                            key_slot
     *       [0,pos)     [pos,pos+n)   [pos+n,num_key+n)
     *                      key           key_slot
     */
    void insert_pairs(int pos, const char *key, const Rid *rid, int n);

    void insert_pair(int pos, const char *key, const Rid &rid);

    void erase_pair(int pos);

    /**
     * @brief 此函数由parent调用，寻找child，返回child在parent中的rid_idx∈[0,page_hdr->num_key)
     */
    int find_child(IxNodeHandle *child);
};

class IxIndexHandle {
    friend class IxScan;
    friend class IxManager;

   private:
    DiskManager *disk_manager_;
    BufferPoolManager *buffer_pool_manager_;
    int fd_;
    IxFileHdr file_hdr_;  // 存了root_page，但其初始化为2（第0页存FILE_HDR_PAGE，第1页存LEAF_HEADER_PAGE）
    std::mutex root_latch_;

   public:
    IxIndexHandle(DiskManager *disk_manager, BufferPoolManager *buffer_pool_manager, int fd);

    // for search
    bool GetValue(const char *key, std::vector<Rid> *result, Transaction *transaction);
    std::pair<IxNodeHandle *, bool> FindLeafPage(const char *key, Operation operation, Transaction *transaction,
                                                 bool find_first = false);

    // for insert
    page_id_t insert_entry(const char *key, const Rid &value, Transaction *transaction);

    void StartNewTree(const char *key, const Rid &value);

    IxNodeHandle *Split(IxNodeHandle *node);

    void InsertIntoParent(IxNodeHandle *old_node, const char *key, IxNodeHandle *new_node, Transaction *transaction);

    // for delete
    void delete_entry(const char *key, const Rid &value, Transaction *transaction);

    bool CoalesceOrRedistribute(IxNodeHandle *node, Transaction *transaction = nullptr,
                                bool *root_is_latched = nullptr);
    bool AdjustRoot(IxNodeHandle *old_root_node);

    void Redistribute(IxNodeHandle *neighbor_node, IxNodeHandle *node, IxNodeHandle *parent, int index);

    bool Coalesce(IxNodeHandle **neighbor_node, IxNodeHandle **node, IxNodeHandle **parent, int index,
                  Transaction *transaction, bool *root_is_latched);

    // 辅助函数，lab3执行层将使用
    Iid lower_bound(const char *key);

    Iid upper_bound(const char *key);

    Iid leaf_end() const;

    Iid leaf_begin() const;

   private:
    // 辅助函数
    void UpdateRootPageNo(page_id_t root);

    bool IsEmpty() const;

    // for concurrent index, using crabbing protocol
    void UnlockPages(Transaction *transaction);

    void UnlockUnpinPages(Transaction *transaction);

    bool IsSafe(IxNodeHandle *node, Operation op);

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
