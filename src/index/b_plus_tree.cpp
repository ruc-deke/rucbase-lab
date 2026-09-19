// Copyright (c) 2023-2027 Renmin University of China
// SPDX-License-Identifier: MulanPSL-2.0

#include "b_plus_tree.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <limits>
#include <memory>

#include "common/errors.h"
#include "index_scan.h"
#include "storage/buffer_pool_manager.h"
#include "storage/disk_manager.h"
#include "storage/page.h"

int compare_index_key(const char* left, const char* right, ColType type, int column_length) {
    if (left == nullptr || right == nullptr || column_length <= 0) {
        throw InternalError("Invalid index comparison input");
    }
    switch (type) {
        case TYPE_INT: {
            if (column_length != static_cast<int>(sizeof(int))) {
                throw InternalError("Invalid integer index key length");
            }
            int left_value;
            int right_value;
            memcpy(&left_value, left, sizeof(left_value));
            memcpy(&right_value, right, sizeof(right_value));
            return (left_value < right_value) ? -1 : ((left_value > right_value) ? 1 : 0);
        }
        case TYPE_FLOAT: {
            if (column_length != static_cast<int>(sizeof(float))) {
                throw InternalError("Invalid float index key length");
            }
            float left_value;
            float right_value;
            memcpy(&left_value, left, sizeof(left_value));
            memcpy(&right_value, right, sizeof(right_value));
            return (left_value < right_value) ? -1 : ((left_value > right_value) ? 1 : 0);
        }
        case TYPE_STRING:
            return memcmp(left, right, column_length);
        default:
            throw InternalError("Unexpected data type");
    }
}

int compare_index_key(const char* left,
                      const char* right,
                      const std::vector<ColType>& column_types,
                      const std::vector<int>& column_lengths) {
    if (column_types.size() != column_lengths.size()) {
        throw InternalError("Invalid composite index metadata");
    }
    int offset = 0;
    for (size_t i = 0; i < column_types.size(); ++i) {
        int result = compare_index_key(left + offset, right + offset, column_types[i], column_lengths[i]);
        if (result != 0) return result;
        offset += column_lengths[i];
    }
    return 0;
}

BPlusTreeNode::BPlusTreeNode(const IndexFileHeader* file_header, Page* page) : file_header_(file_header), page_(page) {
    page_header_ = reinterpret_cast<IndexPageHeader*>(page_->get_data());
    keys_ = page_->get_data() + sizeof(IndexPageHeader);
    rids_ = reinterpret_cast<Rid*>(keys_ + file_header_->keys_region_size_);
}

page_id_t BPlusTreeNode::get_page_no() const noexcept { return page_->get_page_id().page_no; }

PageId BPlusTreeNode::get_page_id() const noexcept { return page_->get_page_id(); }

page_id_t BPlusTreeNode::remove_and_return_only_child() {
    assert(get_size() == 1);
    page_id_t child_page_no = value_at(0);
    erase_pair(0);
    assert(get_size() == 0);
    return child_page_no;
}

int BPlusTreeNode::find_child(const BPlusTreeNode* child) const {
    int child_index = 0;
    while (child_index < page_header_->key_count && get_rid(child_index)->page_no != child->get_page_no()) {
        ++child_index;
    }
    assert(child_index < page_header_->key_count);
    return child_index;
}

/**
 * @brief 在当前node中查找第一个>=target的key_idx
 *
 * @return key_idx，范围为[0,num_key)，如果返回的key_idx=num_key，则表示target大于最后一个key
 * @note 返回key index（同时也是rid index），作为slot no
 */
int BPlusTreeNode::lower_bound(const char* target) const {
    // Todo:
    // 查找当前节点中第一个大于等于target的key，并返回key的位置给上层
    // 提示: 可以采用多种查找方式，如顺序遍历、二分查找等；使用compare_index_key()函数进行比较

    return -1;
}

/**
 * @brief 在当前node中查找第一个>target的key_idx
 *
 * @return key_idx，范围为[0,num_key]，如果返回的key_idx=num_key，则表示target大于等于最后一个key
 * @note 内部结点用它定位孩子时，返回 0 表示 target 比第一个 key 还小（只会出现在最左侧路径上）。
 */
int BPlusTreeNode::upper_bound(const char* target) const {
    // Todo:
    // 查找当前节点中第一个大于target的key，并返回key的位置给上层
    // 提示: 可以采用多种查找方式：顺序遍历、二分查找等；使用compare_index_key()函数进行比较

    return -1;
}

/**
 * @brief 用于叶子结点根据key来查找该结点中的键值对
 * 值value作为传出参数，函数返回是否查找成功
 *
 * @param key 目标key
 * @param[out] value 传出参数，目标key对应的Rid
 * @return 目标key是否存在
 */
bool BPlusTreeNode::leaf_lookup(const char* key, Rid** value) {
    // Todo:
    // 1. 在叶子节点中获取目标key所在位置
    // 2. 判断目标key是否存在
    // 3. 如果存在，获取key对应的Rid，并赋值给传出参数value
    // 提示：可以调用lower_bound()和get_rid()函数。

    return false;
}

/**
 * 用于内部结点（非叶子节点）查找目标key所在的孩子结点（子树）
 * @param key 目标key
 * @return page_id_t 目标key所在的孩子节点（子树）的存储页面编号
 */
page_id_t BPlusTreeNode::internal_lookup(const char* key) {
    // Todo:
    // 1. 查找当前非叶子节点中目标key所在孩子节点（子树）的位置；key 比第一个 key 还小时进入第一个孩子
    // 2. 获取该孩子节点（子树）所在页面的编号
    // 3. 返回页面编号

    return -1;
}

/**
 * @brief 在指定位置插入n个连续的键值对
 * 将key的前n位插入到原来keys中的pos位置；将rid的前n位插入到原来rids中的pos位置
 *
 * @param pos 要插入键值对的位置
 * @param key 连续键数组的起始地址。
 * @param rid 连续 Rid 数组的起始地址。
 * @param n 键值对数量
 * @note [0,pos)           [pos,num_key)
 *                            key_slot
 *                            /      \
 *                           /        \
 *       [0,pos)     [pos,pos+n)   [pos+n,num_key+n)
 *                      key           key_slot
 */
void BPlusTreeNode::insert_pairs(int pos, const char* key, const Rid* rid, int n) {
    // Todo:
    // 1. 判断pos的合法性
    // 2. 通过key获取n个连续键值对的key值，并把n个key值插入到pos位置
    // 3. 通过rid获取n个连续键值对的rid值，并把n个rid值插入到pos位置
    // 4. 更新当前节点的键数量
}

/**
 * @brief 用于在结点中插入单个键值对。
 * 函数返回插入后的键值对数量
 *
 * @param key 要插入的键。
 * @param value 要插入的 Rid。
 * @return int 键值对数量
 */
int BPlusTreeNode::insert(const char* key, const Rid& value) {
    // Todo:
    // 1. 查找要插入的键值对应该插入到当前节点的哪个位置
    // 2. 插入键值对并保持有序；相同 key 可以并存（插在已有相同 key 的后面）
    // 3. 返回完成插入操作之后的键值对数量

    return -1;
}

/**
 * @brief 用于在结点中的指定位置删除单个键值对
 *
 * @param pos 要删除键值对的位置
 */
void BPlusTreeNode::erase_pair(int pos) {
    // Todo:
    // 1. 删除该位置的key
    // 2. 删除该位置的rid
    // 3. 更新结点的键值对数量
}

/**
 * @brief 用于在结点中删除指定key的键值对。函数返回删除后的键值对数量
 *
 * @param key 要删除的键值对key值
 * @return 完成删除操作后的键值对数量
 */
int BPlusTreeNode::remove(const char* key, const Rid& rid) {
    // Todo:
    // 1. 查找 (key, rid) 所在的位置；同一 key 可能有多条，要同时匹配 rid
    // 2. 如果存在则删除该键值对
    // 3. 返回完成删除操作后的键值对数量

    return -1;
}

BPlusTree::BPlusTree(DiskManager* disk_manager, BufferPoolManager* buffer_pool_manager, int file_descriptor)
    : disk_manager_(disk_manager),
      buffer_pool_manager_(buffer_pool_manager),
      file_descriptor_(file_descriptor),
      file_header_() {
    alignas(int) char buf[PAGE_SIZE]{};
    disk_manager_->read_page(file_descriptor_, INDEX_FILE_HEADER_PAGE, buf, PAGE_SIZE);
    auto file_header = std::make_unique<IndexFileHeader>();
    file_header->deserialize(buf);

    // page_count_ is not a physical high-water mark after deletion or a crash.
    // Never reuse a page that is already present in the file.
    int64_t file_size = disk_manager_->get_file_size(file_descriptor_);
    if (file_size < 0) {
        throw InternalError("BPlusTree: cannot stat index file");
    }
    int64_t physical_pages_64 = file_size / PAGE_SIZE + (file_size % PAGE_SIZE != 0);
    if (physical_pages_64 > std::numeric_limits<page_id_t>::max()) {
        throw InternalError("BPlusTree: index file is too large");
    }
    auto physical_pages = static_cast<page_id_t>(physical_pages_64);
    disk_manager_->set_fd2pageno(file_descriptor_, std::max(file_header->page_count_, physical_pages));
    file_header_ = std::move(file_header);
}

/**
 * @brief 用于查找指定键所在的叶子结点
 * @param key 要查找的目标key值
 * @param operation 查找到目标键值对后要进行的操作类型
 * @param transaction 事务参数，如果不需要则默认传入nullptr
 * @param find_first 是否从叶子链表的最左端开始查找
 * @return 目标叶子结点（IndexNode 离开作用域即 unpin）以及根结点是否加锁
 */
std::pair<IndexNode, bool> BPlusTree::find_leaf_page(const char* key,
                                                     IndexOperation operation,
                                                     Transaction* transaction,
                                                     bool find_first) {
    // Todo:
    // 1. 获取根节点
    // 2. 从根节点开始不断向下查找目标key
    // 3. 找到包含该key值的叶子结点停止查找，并返回叶子节点

    return {IndexNode{}, false};
}

/**
 * @brief 查找指定 key 对应的全部 Rid。
 *
 * @param key 查找的目标key值
 * @param result 用于存放结果的容器
 * @param transaction 事务指针
 * @return bool 返回目标键值对是否存在
 * @note 非唯一索引中，同一 key 的记录可能因分裂分布在多个相邻叶子里。
 */
bool BPlusTree::get_value(const char* key, std::vector<Rid>* result, Transaction* transaction) {
    // Todo:
    // 1. 获取目标 key 所在的叶子结点
    // 2. 找到该 key 的全部记录（非唯一索引可能有多条），把 rid 放入 result
    // 思考：同一 key 的记录跨越多个叶子时，第 1 步找到的叶子是否一定包含第一条？
    // 提示：IndexNode 离开作用域即 unpin。

    return false;
}

/**
 * @brief  将传入的一个node拆分(Split)成两个结点，在node的右边生成一个新结点new node
 * @param node 需要拆分的结点
 * @return 拆分得到的new_node
 * @note 返回的 IndexNode 拥有新页的 pin；原 node 仍由调用方持有。
 */
IndexNode BPlusTree::split(IndexNode& node) {
    // Todo:
    // 1. 将原结点的键值对平均分配，右半部分分裂为新的右兄弟结点
    //    需要初始化新节点的page_hdr内容
    // 2. 如果新的右兄弟结点是叶子结点，更新新旧节点的prev_leaf和next_leaf指针
    //    为新节点分配键值对，更新旧节点的键值对数记录
    // 3. 如果新的右兄弟结点不是叶子结点，更新该结点的所有孩子结点的父节点信息(使用BPlusTree::update_child_parent())

    return {};
}

/**
 * @brief Insert key & value pair into internal page after split
 * 拆分(Split)后，向上找到old_node的父结点
 * 将new_node的第一个key插入到父结点，其位置在 父结点指向old_node的孩子指针 之后
 * 如果插入后>=maxsize，则必须继续拆分父结点，然后在其父结点的父结点再插入，即需要递归
 * 直到找到的old_node为根结点时，结束递归（此时将会新建一个根R，关键字为key，old_node和new_node为其孩子）
 *
 * @param old_node 分裂后保留左半部分的原结点。
 * @param key 要插入parent的key
 * @param new_node 分裂产生的右兄弟结点。
 * @param transaction 当前事务；不需要并发控制时可为 nullptr。
 * @note 一个结点插入了键值对之后需要分裂，分裂后左半部分的键值对保留在原结点，在参数中称为old_node，
 * 右半部分的键值对分裂为新的右兄弟节点，在参数中称为new_node（参考Split函数来理解old_node和new_node）
 * @note old_node / new_node 的 pin 仍由调用方的 IndexNode 持有。
 */
void BPlusTree::insert_into_parent(IndexNode& old_node,
                                   const char* key,
                                   IndexNode& new_node,
                                   Transaction* transaction) {
    // Todo:
    // 1. 分裂前的结点（原结点, old_node）是否为根结点，如果为根结点需要分配新的root
    // 2. 获取原结点（old_node）的父亲结点
    // 3. 获取key对应的rid，并将(key, rid)插入到父亲结点
    // 4. 如果父亲结点仍需要继续分裂，则进行递归插入
    // 提示：不要手动 unpin；IndexNode 析构会释放 pin。
}

/**
 * @brief 将指定键值对插入到B+树中
 * @param key 要插入的键。
 * @param value 键对应的 Rid。
 * @param transaction 事务指针
 * @return page_id_t 插入到的叶结点的page_no
 * @throws DuplicateKeyError 唯一索引中已存在相同 key，此时树保持不变。
 * @pre 调用方不会重复插入完全相同的 (key, value)。
 */
page_id_t BPlusTree::insert_entry(const char* key, const Rid& value, Transaction* transaction) {
    // Todo:
    // 1. 若 is_unique() 且 key 已存在，抛出 DuplicateKeyError
    // 2. 查找 (key, value) 应该插入到哪个叶子节点
    // 3. 在该叶子节点中插入键值对
    // 4. 如果结点已满，分裂结点，并把新结点的相关信息插入父节点
    // 提示：IndexNode 离开作用域即 unpin；若当前叶子是最右叶子，更新 file_header_->last_leaf_。

    return -1;
}

/**
 * @brief 删除 B+ 树中的一条 (key, rid)。
 * @param key 要删除的key值
 * @param rid 要删除的记录位置；同一 key 的其他记录保留
 * @param transaction 事务指针
 * @return 该 (key, rid) 存在并被删除时返回 true
 */
bool BPlusTree::delete_entry(const char* key, const Rid& rid, Transaction* transaction) {
    // Todo:
    // 1. 获取 (key, rid) 所在的叶子结点；与 get_value 一样，它可能不在第一次找到的叶子里
    // 2. 只删除这一条键值对（同一 key 的其他记录保留）
    // 3. 如果删除成功需要调用 coalesce_or_redistribute 处理合并或重分配

    return false;
}

/**
 * @brief 用于处理合并和重分配的逻辑，用于删除键值对后调用
 *
 * @param node 执行完删除操作的结点
 * @param transaction 事务指针
 * @param root_is_latched 传出参数：根节点是否上锁，用于并发操作
 * @return 是否需要删除结点
 * @note User needs to first find the sibling of input page.
 * If sibling's size + input page's size >= 2 * page's minsize, then redistribute.
 * Otherwise, merge(Coalesce).
 */
bool BPlusTree::coalesce_or_redistribute(IndexNode& node, Transaction* transaction, bool* root_is_latched) {
    // Todo:
    // 1. 判断node结点是否为根节点
    //    1.1 如果是根节点，需要调用AdjustRoot() 函数来进行处理，返回根节点是否需要被删除
    //    1.2 如果不是根节点，并且不需要执行合并或重分配操作，则直接返回false，否则执行2
    // 2. 获取node结点的父亲结点
    // 3. 寻找node结点的兄弟结点（优先选取前驱结点）
    // 4. 如果node结点和兄弟结点的键值对数量之和，能够支撑两个B+树结点（即node.size+neighbor.size >=
    // NodeMinSize*2)，则只需要重新分配键值对（调用Redistribute函数）
    // 5. 如果不满足上述条件，则需要合并两个结点，将右边的结点合并到左边的结点（调用Coalesce函数）

    return false;
}

/**
 * @brief 用于当根结点被删除了一个键值对之后的处理
 * @param old_root_node 原根节点
 * @return bool 根结点是否需要被删除
 * @note size of root page can be less than min size and this method is only called within coalesce_or_redistribute()
 */
bool BPlusTree::adjust_root(IndexNode& old_root_node) {
    // Todo:
    // 1. 如果old_root_node是内部结点，并且大小为1，则直接把它的孩子更新成新的根结点
    // 2. 如果old_root_node是叶结点，且大小为0，则直接更新root page
    // 3. 除了上述两种情况，不需要进行操作

    return false;
}

/**
 * @brief 重新分配node和兄弟结点neighbor_node的键值对
 * Redistribute key & value pairs from one page to its sibling page. If index == 0, move sibling page's first key
 * & value pair into end of input "node", otherwise move sibling page's last key & value pair into head of input "node".
 *
 * @param neighbor_node sibling page of input "node"
 * @param node input from method coalesceOrRedistribute()
 * @param parent the parent of "node" and "neighbor_node"
 * @param index node在parent中的rid_idx
 * @note node是之前刚被删除过一个key的结点
 * index=0，则neighbor是node后继结点，表示：node(left)      neighbor(right)
 * index>0，则neighbor是node前驱结点，表示：neighbor(left)  node(right)
 * 注意更新parent结点的相关kv对
 */
void BPlusTree::redistribute(IndexNode& neighbor_node, IndexNode& node, IndexNode& parent, int index) {
    // Todo:
    // 1. 通过index判断neighbor_node是否为node的前驱结点
    // 2. 从neighbor_node中移动一个键值对到node结点中
    // 3. 更新父节点中的相关信息，并且修改移动键值对对应孩字结点的父结点信息（update_child_parent函数）
    // 注意：neighbor_node的位置不同，需要移动的键值对不同，需要分类讨论
}

/**
 * @brief 合并(Coalesce)函数是将node和其直接前驱进行合并，也就是和它左边的neighbor_node进行合并；
 * 假设node一定在右边。如果上层传入的index=0，说明node在左边，那么交换node和neighbor_node，保证node在右边；合并到左结点，实际上就是删除了右结点；
 * Move all the key & value pairs from one page to its sibling page, and notify buffer pool manager to delete this page.
 * Parent page must be adjusted to take info of deletion into account. Remember to deal with coalesce or redistribute
 * recursively if necessary.
 *
 * @param neighbor_node sibling page of input "node" (neighbor_node是node的前结点)
 * @param node input from method coalesceOrRedistribute() (node结点是需要被删除的)
 * @param parent parent page of input "node"
 * @param index node在parent中的rid_idx
 * @param transaction 当前事务；不需要并发控制时可为 nullptr。
 * @param root_is_latched 传出根结点的加锁状态。
 * @return true means parent node should be deleted, false means no deletion happend
 * @note Assume that *neighbor_node is the left sibling of *node (neighbor -> node)
 */
bool BPlusTree::coalesce(IndexNode& neighbor_node,
                         IndexNode& node,
                         IndexNode& parent,
                         int index,
                         Transaction* transaction,
                         bool* root_is_latched) {
    // Todo:
    // 1. 用index判断neighbor_node是否为node的前驱结点，若不是则交换两个结点，让neighbor_node作为左结点，node作为右结点
    // 2. 把node结点的键值对移动到neighbor_node中，并更新node结点孩子结点的父节点信息（调用update_child_parent函数）
    // 3. 释放和删除node结点，并删除parent中node结点的信息，返回parent是否需要被删除
    // 提示：如果是叶子结点且为最右叶子结点，需要更新file_header_.last_leaf

    return false;
}

/**
 * @brief 将索引扫描位置转换为表记录位置 Rid。
 *
 * @param position 叶子页号和页内槽号。
 * @return 对应索引项保存的 Rid。
 * @note IndexPosition 描述索引内部位置，Rid 描述表记录位置，二者含义不同。
 */
Rid BPlusTree::get_rid(const IndexPosition& position) const {
    IndexNode node = fetch_node(position.page_no);
    if (!node.valid() || position.slot_no < 0 || position.slot_no >= node->get_size()) {
        throw IndexEntryNotFoundError();
    }
    return *node->get_rid(position.slot_no);
}

/**
 * @brief 返回整棵树中第一个 key >= 目标 key 的索引项位置。
 *
 * @param key 目标 key
 * @return IndexPosition 可作为 IndexScan 的起点
 * @note 位置规范：若结果恰好落在某个非最后叶子的末尾（slot_no == size），应改为下一叶子的
 *       第 0 个槽；若所有 key 都小于目标 key，返回 leaf_end()。这样得到的位置才能与
 *       IndexScan::next() 产生的位置直接比较。
 */
IndexPosition BPlusTree::lower_bound(const char* key) {
    // Todo:
    // 1. 找到可能包含目标 key 的叶子，并在叶子内定位
    // 2. 按上面的位置规范调整返回值
    // 思考：非唯一索引中，“第一个 >= key”的索引项可能在哪个叶子里？

    return IndexPosition{.page_no = INDEX_NO_PAGE, .slot_no = -1};
}

/**
 * @brief 返回整棵树中第一个 key > 目标 key 的索引项位置。
 *
 * @param key 目标 key
 * @return IndexPosition 可作为 IndexScan 的终点
 * @note 位置规范与 lower_bound() 相同。
 */
IndexPosition BPlusTree::upper_bound(const char* key) {
    // Todo:
    // 1. 找到可能包含目标 key 的叶子，并在叶子内定位
    // 2. 按 lower_bound() 的位置规范调整返回值

    return IndexPosition{.page_no = INDEX_NO_PAGE, .slot_no = -1};
}

/**
 * @brief 指向最后一个叶子的最后一个结点的后一个
 * 用处在于可以作为IndexScan的最后一个
 *
 * @return IndexPosition
 */
IndexPosition BPlusTree::leaf_end() const {
    IndexNode node = fetch_node(file_header_->last_leaf_);
    if (!node.valid()) {
        throw InternalError("failed to fetch last leaf for index scan");
    }
    return {.page_no = file_header_->last_leaf_, .slot_no = node->get_size()};
}

/**
 * @brief 指向第一个叶子的第一个结点
 * 用处在于可以作为IndexScan的第一个
 *
 * @return IndexPosition
 */
IndexPosition BPlusTree::leaf_begin() const { return {.page_no = file_header_->first_leaf_, .slot_no = 0}; }

IndexNode BPlusTree::fetch_node(page_id_t page_no) const {
    PageGuard guard = buffer_pool_manager_->fetch_page_guard(PageId{.fd = file_descriptor_, .page_no = page_no});
    return {file_header_.get(), std::move(guard)};
}

IndexNode BPlusTree::create_node() {
    PageId new_page_id = {.fd = file_descriptor_, .page_no = INVALID_PAGE_ID};
    PageGuard guard = buffer_pool_manager_->new_page_guard(&new_page_id);
    if (!guard) {
        return {};
    }
    file_header_->page_count_++;
    return {file_header_.get(), std::move(guard)};
}

void BPlusTree::update_ancestor_keys(IndexNode& node) {
    page_id_t current_page = node->get_page_no();
    while (true) {
        IndexNode current = fetch_node(current_page);
        if (!current.valid() || current->get_parent_page_no() == INDEX_NO_PAGE) {
            break;
        }
        IndexNode parent = fetch_node(current->get_parent_page_no());
        if (!parent.valid()) {
            break;
        }
        const int rank = parent->find_child(&current.node());
        if (memcmp(parent->get_key(rank), current->get_key(0), file_header_->key_length_) == 0) {
            break;
        }
        parent->set_key(rank, current->get_key(0));
        parent.mark_dirty();
        current_page = parent->get_page_no();
    }
}

// 框架辅助：调用即可。这段失败回滚是框架内部实现，作业不要模仿。
void BPlusTree::unlink_leaf(IndexNode& leaf) {
    if (!leaf.valid() || !leaf->is_leaf_page()) {
        throw InternalError("unlink_leaf requires a valid leaf node");
    }

    const page_id_t leaf_page = leaf->get_page_no();
    const page_id_t prev_page = leaf->get_prev_leaf();
    const page_id_t next_page = leaf->get_next_leaf();

    {
        IndexNode prev = fetch_node(prev_page);
        if (!prev.valid()) {
            throw InternalError("unlink_leaf failed to fetch previous leaf");
        }
        prev->set_next_leaf(next_page);
        prev.mark_dirty();
    }
    {
        IndexNode next = fetch_node(next_page);
        if (!next.valid()) {
            IndexNode prev = fetch_node(prev_page);
            if (prev.valid()) {
                prev->set_next_leaf(leaf_page);
                prev.mark_dirty();
            }
            throw InternalError("unlink_leaf failed to fetch next leaf; previous link rolled back");
        }
        next->set_prev_leaf(prev_page);
        next.mark_dirty();
    }
}

void BPlusTree::record_page_deletion() { file_header_->page_count_--; }

void BPlusTree::update_child_parent(IndexNode& node, int child_idx) {
    if (!node.valid() || node->is_leaf_page()) {
        return;
    }
    IndexNode child = fetch_node(node->value_at(child_idx));
    if (!child.valid()) {
        return;
    }
    child->set_parent_page_no(node->get_page_no());
    child.mark_dirty();
}
