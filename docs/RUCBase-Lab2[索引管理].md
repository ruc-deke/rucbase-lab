# Lab 2：索引管理

开始实验前，请先按照 [RUCBase 使用文档](RUCBase使用文档.md) 完成构建和测试环境配置。

在本实验中，学生需要完成 B+ 树索引的核心算法。开始编码前，可先阅读 [`src/index/README.md`](../src/index/README.md) 了解模块边界和页面布局。

| 文件 | 主要类型 | 职责 |
| --- | --- | --- |
| `index_types.h/.cpp` | `IndexFileHeader`、`IndexPageHeader`、`IndexPosition` | 定义磁盘布局和扫描位置 |
| `index_manager.h/.cpp` | `IndexManager` | 管理索引文件生命周期 |
| `b_plus_tree.h/.cpp` | `BPlusTree`、`BPlusTreeNode` | 实现 B+ 树算法 |
| `index_scan.h/.cpp` | `IndexScan` | 沿叶子链表遍历索引项 |

学生只需要实现 `BPlusTree` 和 `BPlusTreeNode` 中标有 `Todo` 的接口；`IndexManager`、`IndexScan` 和序列化代码均由框架提供。

## 普通索引与唯一索引（UNIQUE）

本实验的 B+ 树要同时支持两种索引：

| 索引类型 | 创建方式 | 同一 key 的记录数 | 插入重复 key 时 |
| --- | --- | --- | --- |
| 普通索引 | `CREATE INDEX 表名 (列名, ...)` | 任意多条 | 正常插入 |
| 唯一索引 | `CREATE UNIQUE INDEX 表名 (列名, ...)` | 至多一条 | 抛出 `DuplicateKeyError`，树保持不变 |

索引中存放的是 `(key, Rid)` 键值对：`key` 是索引列的取值（复合索引按列顺序拼接），`Rid` 是该记录在表文件中的位置。两种索引的页面布局完全相同，区别只在于是否允许重复 key。

**唯一性从哪里来。** 创建索引时，`IndexMeta::make(表, 列, unique)` 记录这条索引是否唯一；`IndexManager::create_index` 把它写入索引文件头 `IndexFileHeader::unique_`。在 `BPlusTree` 中调用 `is_unique()` 即可读取，不需要自己保存。本实验的单元测试直接用 `IndexMeta::make(表, 列, true)` 创建唯一索引；SQL 层的 `CREATE UNIQUE INDEX` 在 Lab 3 中接入和测试。

**本实验需要做到：**

1. **唯一约束**：`insert_entry()` 在唯一索引中遇到已存在的 key 时，抛出 `DuplicateKeyError`（定义在 `common/errors.h`），不能插入，也不能留下任何修改（包括分裂出的结点和未释放的 pin）。
2. **重复 key**：普通索引中，`insert()` 把新记录插在已有相同 key 的后面；`get_value()` 返回该 key 的**全部** `Rid`。
3. **按 `(key, Rid)` 删除**：`delete_entry(key, rid, txn)` 只删除 key 和 rid 都匹配的那一条，同一 key 的其他记录保留；不存在时返回 `false`。
4. **跨叶子的重复 key**：同一 key 的多条记录会因为结点分裂分布在**多个相邻叶子**中，查找、删除和范围扫描都要处理这种情况。

以阶数 3（每个结点最多存 3 个键值对）为例，依次插入 `(5,r1) (7,r2) (7,r3) (7,r4) (7,r5)` 后，树可能是：

```text
             [5 | 7]
            /       \
       [5, 7]  -->  [7, 7, 7]
```

key = 7 的 4 条记录分布在两个叶子中，父结点中 key 7 左边的子树里也有 7。如果查找 7 时只按父结点里的 key 7 进入右边的孩子，就会漏掉左边叶子里的那条记录。怎样找到并处理这些记录由你自己设计；叶子之间可以通过 `get_prev_leaf()` / `get_next_leaf()` 互相访问。

上述要求由 `src/test/index/b_plus_tree_duplicate_test.cpp` 测试（15 分），详见文末“实验计分”。

![Lab 2 索引管理实验流程图](pics/Lab2流程图.png)

B+树的结构如图：

![B+树的结构](pics/B+树的结构.png)

注意：

（1）索引按 `(key, Rid)` 存放，普通索引允许重复 key，唯一索引不允许，详见上文“普通索引与唯一索引（UNIQUE）”。

（2）结点能容纳的键值对数量小于最大值，大于等于最小值。这相当于留出了一个多余的空位，方便B+树进行插入和删除操作。

（3）在上图中，value的数量比key的数量多一个，而为了体现”键值对“的概念，实际上应该将key和value的数量设为相等。为了做到这一点，在每个内部结点的第一个值前面额外加上第一个键，这样就能让键和值的数量保持一致，即具有 k+1 个键的内部结点能索引 k+1 个子树。这个键（内部结点的第一个键）存储的key设置为其第一个孩子结点的第一个key，当结点分裂或合并时，可能需要更新其信息；同样地，在每个叶子结点的第一个值前面也额外加上第一个键，它存储当前结点中插入的最小key。（**每个结点的第一个key，存储的都是以该结点为根结点的子树中的所有key的最小值**）

**辅助函数说明**

本实验提供一些已经实现好的辅助函数，学生无需实现，可以阅读其实现，并调用其功能。

（1）`BPlusTreeNode`类的辅助函数：

```cpp
class BPlusTreeNode {
    // 辅助函数（本实验提供，无需实现）
    char *get_key(int key_idx) const;
    Rid *get_rid(int rid_idx) const;
}
```

- `char *get_key(int key_idx) const;`

​		得到键数组中指定位置的地址。

- `Rid *get_rid(int rid_idx) const;`

​		得到值数组中指定位置的地址。

（2）`BPlusTree`类辅助函数：

```cpp
class BPlusTree {
    // 辅助函数（本实验提供，无需实现）
    IndexNode fetch_node(page_id_t page_no) const;
    IndexNode create_node();
    void update_ancestor_keys(IndexNode &node);
    void update_child_parent(IndexNode &node, int child_idx);
    void unlink_leaf(IndexNode &leaf);
    void record_page_deletion();
};
```

- `IndexNode fetch_node(page_id_t page_no) const;`

  用于获取指定页面对应的结点。通过 `node->` 访问键和值，修改后调用 `node.mark_dirty()`。结点离开作用域后，对应页面会自动解除固定。

- `IndexNode create_node();`

  用于创建一个新结点。

```cpp
IndexNode node = fetch_node(page_no);
node->insert(...);
node.mark_dirty();
```

- `void update_ancestor_keys(IndexNode &node);`

  用于从`node`开始更新其父节点的第一个key，一直向上更新直到根节点。

- `void update_child_parent(IndexNode &node, int child_idx);`

  用于将`node`的第`child_idx`个孩子结点的父结点指针置为`node`。

- `void unlink_leaf(IndexNode &leaf);`

  用于删除 `leaf` 之前，更新其前驱和后继叶子页指针。

- `void record_page_deletion();`

  用于删除`node`之后，更新索引头记录的页面个数信息。

（3）`int compare_index_key(const char *left, const char *right, ColType type, int column_length);`

​		用于比较两个定长键。`column_length` 是该列在复合键中占用的字节数。

### 任务1 B+树的查找

#### （1）结点内的查找

```cpp
class BPlusTreeNode {
    // 结点内的查找
    int lower_bound(const char *target) const;
    int upper_bound(const char *target) const;
    bool leaf_lookup(const char *key, Rid **value);
    page_id_t internal_lookup(const char *key);
}
```

为了实现整个B+树的查找，首先需要实现B+树单个结点内部的查找。

学生需要实现以下函数：

- `int lower_bound(const char *target) const;`

  用于在当前结点中查找第一个大于或等于`target`的key的位置。

- `int upper_bound(const char *target) const;`

  用于在当前结点中查找第一个大于`target`的key的位置。

提示：获得key需要调用`get_key()`函数；在比较key大小时需要调用`compare_index_key()`函数；B+树中每个结点的键数组是有序的，可用二分查找。

- `bool leaf_lookup(const char *key, Rid **value);`

​		用于叶子结点根据key来查找该结点中的键值对。值`value`作为传出参数，函数返回是否查找成功。

​		提示：可以调用`lower_bound()`和`get_rid()`函数。

- `page_id_t internal_lookup(const char *key);`

​		用于内部结点根据key来查找该key所在的孩子结点（子树）。

​		值value为Rid类型，对于内部结点，其Rid中的page_no表示指向的孩子结点的页面编号。而内部结点每个key右边的value指向的孩子结点中的键均大于等于该key，每个key左边的value指向的孩子结点中的键均小于等于该key（唯一索引中为严格小于）。根据这一特性，思考如何找到key所在的孩子结点。

​		提示：可以调用`upper_bound()`和`get_rid()`函数。注意目标 key 比第一个 key 还小的情况（例如插入新的最小值），此时应进入第一个孩子。

#### （2）B+树的查找

```cpp
class BPlusTree {
    std::pair<IndexNode, bool> find_leaf_page(const char *key, IndexOperation operation, Transaction *transaction,
                                              bool find_first = false);
    bool get_value(const char *key, std::vector<Rid> *result, Transaction *transaction);
    IndexPosition lower_bound(const char *key);
    IndexPosition upper_bound(const char *key);
};
```

学生需要实现以下函数：

- `std::pair<IndexNode, bool> find_leaf_page(const char *key, IndexOperation operation, Transaction *transaction, bool find_first = false);`

​		用于查找指定键所在的叶子结点。

​		从根结点开始，不断向下查找孩子结点，直到找到包含该key的叶子结点。

​		`operation`表示上层调用此函数时进行的是何种操作（因为查找/插入/删除均需要查找叶子结点）。

​		提示：可以调用`fetch_node()`和`internal_lookup()`函数。

- `bool get_value(const char *key, std::vector<Rid> *result, Transaction *transaction);`

  用于查找指定键在叶子结点中的对应的值`result`。非唯一索引时，同一 key 可能有多条记录，需要全部放入 `result`，即使它们分布在多个叶子中。

  提示：可以调用`find_leaf_page()`、`lower_bound()` / `upper_bound()` 或 `leaf_lookup()`。叶子之间可以通过 `get_prev_leaf()` / `get_next_leaf()` 互相访问。

- `IndexPosition lower_bound(const char *key);` / `IndexPosition upper_bound(const char *key);`

  `BPlusTree` 上的同名函数，返回整棵树中第一个 key 大于等于 / 大于目标 key 的索引项位置，用 `IndexPosition{叶子页号, 槽号}` 表示。`[lower_bound(k), upper_bound(k))` 就是 key 等于 `k` 的全部索引项，可以直接交给 `IndexScan` 遍历，后续实验的索引扫描也依赖它。

  位置规范：如果结果恰好落在某个非最后叶子的末尾（`slot_no == size`），要改成下一个叶子的第 0 个槽；如果所有 key 都小于目标 key，返回 `leaf_end()`。只有这样，返回的位置才能和 `IndexScan::next()` 走到的位置直接比较。

### 任务2 B+树的插入

#### （1）结点内的插入

```cpp
class BPlusTreeNode {
    // 结点内的插入
    void insert_pairs(int pos, const char *key, const Rid *rid, int n);
    int insert(const char *key, const Rid &value);
}
```

学生需要实现以下函数：

- `void insert_pairs(int pos, const char *key, const Rid *rid, int n);`

​		用于在结点中的指定位置插入多个键值对。

​		该函数插入指定 `n` 个键值对数组 `(key, rid)` 到结点中的 `pos` 位置。`key` 指向连续的定长复合键，每个键占 `file_header_->key_length_` 字节；`rid` 指向与键一一对应的 `Rid` 数组。节点页内先连续存放键数组，再存放 `Rid` 数组。

​		对于该操作的内部实现逻辑，可以先将数组中原来从第`pos`位开始到其后`n`位的数据移到末尾，再将要插入的数组移到`pos`位之后。注意键数组和值数组的数据都要移动。

​		提示：需要调用`get_key()` / `get_rid()`函数得到 键/值 数组中指定位置的地址；可以调用`memcpy()`和`memmove()`进行数据移动。

- `int insert(const char *key, const Rid &value);`

​		用于在结点中插入单个键值对。函数返回插入后的键值对数量。

​		插入后需要保持键数组仍然有序。相同 key 可以并存，新记录插在已有相同 key 的后面。

​		提示：可以调用`lower_bound()` / `upper_bound()`和`insert_pairs()`函数。结点只负责有序存放键值对，唯一约束在 `insert_entry` 里处理。

#### （2）B+树的插入

```cpp
class BPlusTree {
    page_id_t insert_entry(const char *key, const Rid &value, Transaction *transaction);
    IndexNode split(IndexNode &node);
    void insert_into_parent(IndexNode &old_node, const char *key, IndexNode &new_node, Transaction *transaction);
};
```
学生需要实现以下函数：

- `page_id_t insert_entry(const char *key, const Rid &value, Transaction *transaction);`

​		用于将指定键值对插入到B+树。

​		首先找到要插入的叶结点，然后将键值对插入到该叶结点。如果该结点插入后已满，即size==max_size，就需要分裂成两个结点，分裂后还需要将新结点相关信息插入到父结点，不断向上递归插入直到当前结点在插入后未满或到达根结点。

​		若 `is_unique()` 为真且树中已有相同 key，抛出 `DuplicateKeyError`，不要插入。上层不会重复插入完全相同的 `(key, rid)`，无需处理这种情况。

​		提示：需要调用`find_leaf_page()`、`insert()`、`split()`、`insert_into_parent()`。

- `IndexNode split(IndexNode &node);`

​		用于分裂结点。函数返回分裂产生的新结点。

​		具体做法是，将原结点的键值对平均分配，其左半部分不变，右半部分移动到分裂产生的新结点中。新结点在原结点的右边。

​		注意：如果分裂的结点是叶结点，要更新叶结点的后继指针。如果分裂的结点是内部结点，要更新其孩子结点的父指针。

- `void insert_into_parent(IndexNode &old_node, const char *key, IndexNode &new_node, Transaction *transaction);`

​		用于结点分裂后，更新父结点中的键值对。

​		将`new_node`的第一个key插入到父结点，其位置在 父结点指向`old_node`的孩子指针value 之后。如果父结点插入后size==maxsize，则必须继续分裂父结点，然后在该父结点的父结点再插入，即需要递归。不断地分裂和向上插入，直到父结点被插入后未满，或者一直向上插入到了根结点，才会停止递归；如果一直向上插入到了根结点，会产生一个新的根结点，它的左孩子是分裂前的原结点，右孩子是分裂后产生的新结点。

​		提示：需要调用`split()`和`insert_into_parent()`，进行递归。



B+树插入的整体流程如下图：


![B+树插入流程](pics/B+树插入流程.png)

### 任务3 B+树的删除

#### （1）结点内的删除

```cpp
class BPlusTreeNode {
    // 结点内的删除
    void erase_pair(int pos);
    int remove(const char *key, const Rid &rid);
}
```

学生需要实现以下函数：

- `void erase_pair(int pos);`

  用于在结点中的指定位置删除单个键值对。

  提示：可以调用`memmove()`函数。

- `int remove(const char *key, const Rid &rid);`

​		用于在结点中删除指定的 `(key, rid)`。同一 key 可能有多条，必须同时匹配 rid。函数返回删除后的键值对数量。

​		提示：可以调用`lower_bound()`和`erase_pair()`函数。

#### （2）B+树的删除

```cpp
class BPlusTree {
    bool delete_entry(const char *key, const Rid &rid, Transaction *transaction);
    bool coalesce_or_redistribute(IndexNode &node, Transaction *transaction = nullptr, bool *root_is_latched = nullptr);
    bool coalesce(IndexNode &neighbor_node, IndexNode &node, IndexNode &parent, int index, Transaction *transaction,
                  bool *root_is_latched);
    void redistribute(IndexNode &neighbor_node, IndexNode &node, IndexNode &parent, int index);
    bool adjust_root(IndexNode &old_root_node);
};
```

学生需要实现以下函数：

- `bool delete_entry(const char *key, const Rid &rid, Transaction *transaction);`

​		用于删除B+树中的一条 `(key, rid)`。该键值对存在并被删除时返回 `true`，不存在时返回 `false`。

​		首先找到该键值对所在的叶结点，只删除这一条。与 `get_value()` 一样，要删除的记录可能不在第一次找到的叶子里。如果删除后该结点小于半满，则需要合并（Coalesce）或重分配（Redistribute）。

​		提示：需要调用`find_leaf_page()`、`remove()`、`coalesce_or_redistribute()`。

- `bool coalesce_or_redistribute(IndexNode &node, Transaction *transaction = nullptr, bool *root_is_latched = nullptr);`

  用于处理合并和重分配的逻辑。函数返回是否有结点被删除（无论是`node`还是它的兄弟结点被删除）。传出参数`root_is_latched`记录根结点是否被上锁，该参数将在任务3使用，在本任务2中不使用。

​		首先需要得到`node`的兄弟结点（尽量找前驱结点），然后根据键值对总和能否支撑两个结点决定是合并还是重分配。如果`node`是根结点，则需要特殊处理（AdjustRoot）。

​		提示：需要调用`coalesce()`、`redistribute()`、`adjust_root()`。

- `bool coalesce(IndexNode &neighbor_node, IndexNode &node, IndexNode &parent, int index, Transaction *transaction, bool *root_is_latched);`

​		将`node`向前合并到其前驱`neighbor_node`。函数返回`node`的父结点`parent`否需要被删除。

​		将`node`中的键值对全部移动到`neighbor_node`，移动时注意更新孩子结点的父指针。由于合并操作实质上删除了结点`node`，所以还要删除父结点中的对应键值对，然后继续递归，进入父结点进行合并或重分配。如果是叶结点被删除要更新其后继指针。

​		参数`index`是`node`在`parent`中的rid_idx，其表示`neighbor_node`是否为`node`的前驱结点。需要保证`neighbor_node`为`node`的前驱，如果不是，则交换位置。

​		提示：需要调用`insert_pairs()`、`erase_pair()`、`update_child_parent()`、`record_page_deletion()`。以及`coalesce_or_redistribute()`进行继续递归。

- `void redistribute(IndexNode &neighbor_node, IndexNode &node, IndexNode &parent, int index);`

​		重新分配`node`和兄弟结点`neighbor_node`的键值对。参数`index`表示`node`在parent中的rid_idx，其决定`neighbor_node`是否为`node`的前驱结点。

​		`node`是之前被删除过的结点，所以要移动其兄弟结点`neighbor_node`的一个键值对到`node`。注意这里有多种情况要考虑：根据`neighbor_node`是在`node`的前面还是后面，移动的键值对不一样；此外，如果`node`是内部结点要更新其孩子结点的父指针。

​		提示：需要调用`insert_pairs()`、`erase_pair()`、`update_child_parent()`。

- `bool adjust_root(IndexNode &old_root_node);`

​		用于根结点被删除了一个键值对之后的处理。函数返回根结点是否需要被删除。

​		考虑两种根结点需要被删除的情况：（1）删除了根结点的最后一个键值对，但它仍然有一个孩子。那么可以将其孩子作为新的根结点。（2）删除了整个B+树的最后一个键值对。那么直接更新文件头中记录的根结点为`INVALID_PAGE_ID`。

​		对于其他情况则无需任何处理，因为根结点无需被删除。

​		提示：需要调用`record_page_deletion()`。



B+树删除的整体流程如下图：

![B+树删除流程](pics/B+树删除流程.png)

### 任务4 B+树索引并发控制

本任务要求修改`BPlusTree`类的原实现逻辑，让其支持对B+树索引的**并发**查找、插入、删除操作。

学生可以选择实现并发的粒度，选择下面两种并发粒度的任意一种进行实现即可。

##### 方法一、粗粒度并发（推荐）

对整棵树加一把锁，让查找、插入、删除互斥。实现方式类似于实验一缓冲池管理器的并发控制，足以通过本任务测试。

##### 方法二、细粒度并发（选做）

请自行学习 B+ 树索引并发算法：**蟹行协议（crabbing protocol）**。建议先用方法一通过测试，再尝试本方法。

蟹行协议用读写锁控制对树结点的访问：向下遍历时，先锁住孩子结点，再决定是否释放父结点的锁。

- 查找：进入每一层时先加读锁，再释放父结点的读锁。
- 插入 / 删除：进入每一层时先加写锁；如果当前结点是“安全”的，再释放所有祖先结点的写锁。安全是指：再插入一个键后仍然未满，或再删除一个键后仍然不低于半满。

可以自行保存从根到当前结点经过的祖先。`find_leaf_page` 的返回值是 `std::pair<IndexNode, bool>`，分别表示找到的叶结点和当前是否仍持有根锁。

### 实验计分

本实验满分为100分，测试文件对应的任务点及其分值如下：

| 任务点                         | 测试文件                                        | 分值 |
| ------------------------------ | ----------------------------------------------- | ---- |
| 任务1和任务2  B+树的查找和插入 | src/test/index/b_plus_tree_insert_test.cpp      | 25   |
| 任务3 B+树的删除               | src/test/index/b_plus_tree_delete_test.cpp      | 35   |
| 任务1～3 重复键与唯一约束      | src/test/index/b_plus_tree_duplicate_test.cpp   | 15   |
| 任务4 B+树的并发控制           | src/test/index/b_plus_tree_concurrent_test.cpp  | 25   |

在仓库根目录编译并运行本实验的测试：

```bash
cmake --preset debug
cmake --build --preset debug -j 4
ctest --preset lab2
```

注意：
1. 本实验的测试调用 `get_value()`、`insert_entry()`、`delete_entry(key, rid, txn)` 以及 `BPlusTree::lower_bound()` / `upper_bound()`。学生可以自行添加和修改辅助函数，但不能修改以上函数的声明。
2. 测试会用不变式检查器验证树结构：结点大小、父指针、叶子链表、每个结点的第一个 key 等于子树最小值、key 的顺序（唯一索引严格递增，普通索引非递减），并检查缓冲池中没有遗留的 pin。
3. 索引单元测试直接使用 `IndexManager` 创建测试索引，不要求提前实现 Lab3 的 `SmManager::create_index()`。SQL 层的 `CREATE UNIQUE INDEX` 在 Lab3 中测试。
