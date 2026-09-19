# 索引模块导读

`src/index` 实现基于 B+ 树的二级索引，默认允许重复 key，也可以声明为唯一索引。Lab 2 只要求补全 `BPlusTreeNode` 和 `BPlusTree` 中标有 `Todo` 的算法；文件管理、磁盘布局和顺序扫描由框架提供。

## 文件职责

| 文件                   | 职责                                         | 是否属于 Lab 2 核心实现 |
|------------------------|----------------------------------------------|-------------------------|
| `index_types.h/.cpp`   | 定义索引文件头、节点页头、扫描位置及其序列化 | 否                      |
| `index_manager.h/.cpp` | 创建、打开、关闭和删除索引文件               | 否                      |
| `b_plus_tree.h/.cpp`   | 页内操作、B+ 树查找、插入、删除和并发控制    | 是                      |
| `index_scan.h/.cpp`    | 沿叶子链表遍历索引项                         | 否                      |

建议按 `index_types` → `BPlusTreeNode` / `BPlusTree` 的顺序阅读。`IndexScan` 和文件管理是框架代码。

## 页面布局

索引文件的前三页由框架预留：

| 页号 | 内容                           |
|------|--------------------------------|
| 0    | `IndexFileHeader` 的序列化数据 |
| 1    | 叶子链表哨兵页                 |
| 2    | 初始根页，也是空树的唯一叶子页 |

普通节点页依次存放 `IndexPageHeader`、定长键数组和 `Rid` 数组。键与 `Rid` 使用相同下标一一对应；内部节点的 `Rid::page_no` 表示孩子页号，叶节点的 `Rid` 表示表记录位置。

## 对象关系

- `IndexManager` 负责索引文件的创建、打开和关闭，不实现 B+ 树算法。
- `BPlusTree` 对应一棵已打开的索引。
- `fetch_node` / `create_node` 返回 `IndexNode`。用 `node->` 访问结点中的键和值；结点离开作用域后，对应页面会自动解除固定。
- 树里存的是 `(key, Rid)`。默认同一 key 可以有多条；`IndexMeta::unique` / `file_header_->unique_` 为真时，插入重复 key 抛出 `DuplicateKeyError`。删除按 `(key, Rid)` 指定一条。
- 同一 key 的多条记录可能因为分裂分布在多个相邻叶子中，查找、删除和范围扫描都要考虑这种情况。

具体实验要求和接口说明见 [Lab 2：索引管理](<../../docs/RUCBase-Lab2[索引管理].md>)。
