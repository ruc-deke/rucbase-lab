#pragma once
#include "ix_defs.h"

static const bool binary_search = true;  // 控制在lower_bound/uppper_bound函数中是否使用二分查找

/**
 * @brief 用于比较两个指针指向的数组（类型支持int*、float*、char*）
 */
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

/**
 * @brief 树中的结点
 * 记录了root page，max size等；以及实现结点内部的查找/插入/删除操作
 * 可类比RmPageHandle
 */
class IxNodeHandle {
    friend class IxIndexHandle;
    friend class IxScan;

   private:
    const IxFileHdr *file_hdr;  // 用到了file_hdr的keys_size, col_len
    Page *page;

    /** page->data的第一部分，指针指向首地址，后续占用长度为sizeof(IxPageHdr) */
    IxPageHdr *page_hdr;
    /** page->data的第二部分，指针指向首地址，后续占用长度为file_hdr->keys_size，每个key的长度为file_hdr->col_len */
    char *keys;
    /** page->data的第三部分，指针指向首地址，每个rid的长度为sizeof(Rid) */
    Rid *rids;

   public:
    IxNodeHandle(const IxFileHdr *file_hdr_, Page *page_) : file_hdr(file_hdr_), page(page_) {
        page_hdr = reinterpret_cast<IxPageHdr *>(page->GetData());
        keys = page->GetData() + sizeof(IxPageHdr);
        rids = reinterpret_cast<Rid *>(keys + file_hdr->keys_size);
    }

    IxNodeHandle() = default;

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
     * @return key_idx，范围为[1,num_key)，如果返回的key_idx=num_key，则表示target大于等于最后一个key
     * @note 注意此处的范围从1开始
     */
    int upper_bound(const char *target) const;

    bool LeafLookup(const char *key, Rid **value);

    page_id_t InternalLookup(const char *key);

    /**
     * @brief used in leaf node to insert (key,value)
     *
     * @return the size after Insert
     */
    int Insert(const char *key, const Rid &value);

    /**
     * @brief used in leaf node to remove (key,value) which contains the key
     *
     * @return the size after Remove
     */
    int Remove(const char *key);

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
     * @brief  此函数由parent调用，寻找child
     *
     * @return 返回child在parent中的rid_idx∈[0,page_hdr->num_key)
     */
    int find_child(IxNodeHandle *child);

    /** 以下为已经实现了的辅助函数 **/
    char *get_key(int key_idx) const { return keys + key_idx * file_hdr->col_len; }

    Rid *get_rid(int rid_idx) const { return &rids[rid_idx]; }

    void set_key(int key_idx, const char *key) { memcpy(keys + key_idx * file_hdr->col_len, key, file_hdr->col_len); }

    void set_rid(int rid_idx, const Rid &rid) { rids[rid_idx] = rid; }

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
};