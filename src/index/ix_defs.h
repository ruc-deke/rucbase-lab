#pragma once

#include "defs.h"
#include "storage/buffer_pool_manager.h"

struct IxFileHdr {
    page_id_t first_free_page_no;
    int num_pages;        // disk pages
    page_id_t root_page;  // root page no
    ColType col_type;
    int col_len;      // ColMeta->len
    int btree_order;  // children per page 每个结点最多可插入的键值对数量
    int keys_size;  // keys_size = (btree_order + 1) * col_len
    // first_leaf初始化之后没有进行修改，只不过是在测试文件中遍历叶子结点的时候用了
    page_id_t first_leaf;  // 在上层IxManager的open函数进行初始化，初始化为root page_no
    page_id_t last_leaf;
};

struct IxPageHdr {
    page_id_t next_free_page_no;
    page_id_t parent;  // its parent's page_no
    int num_key;  // # current keys (always equals to #child - 1) 已插入的keys数量，key_idx∈[0,num_key)
    bool is_leaf;
    page_id_t prev_leaf;  // previous leaf node's page_no, effective only when is_leaf is true
    page_id_t next_leaf;  // next leaf node's page_no, effective only when is_leaf is true
};

// 这个其实和Rid结构类似
struct Iid {
    int page_no;
    int slot_no;

    friend bool operator==(const Iid &x, const Iid &y) { return x.page_no == y.page_no && x.slot_no == y.slot_no; }

    friend bool operator!=(const Iid &x, const Iid &y) { return !(x == y); }
};

constexpr int IX_NO_PAGE = -1;
constexpr int IX_FILE_HDR_PAGE = 0;
constexpr int IX_LEAF_HEADER_PAGE = 1;
constexpr int IX_INIT_ROOT_PAGE = 2;
constexpr int IX_INIT_NUM_PAGES = 3;
constexpr int IX_MAX_COL_LEN = 512;
