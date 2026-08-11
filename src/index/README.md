# 索引模块导读

`src/index` 实现基于 B+ 树的唯一索引。Lab 2 只要求补全 `BPlusTreeNode` 和 `BPlusTree` 中标有 `Todo` 的算法；文件管理、磁盘布局和顺序扫描均为教学框架代码。

## 文件职责

| 文件 | 职责 | 是否属于 Lab 2 核心实现 |
| --- | --- | --- |
| `index_types.h/.cpp` | 定义索引文件头、节点页头、扫描位置及其序列化 | 否 |
| `index_manager.h/.cpp` | 创建、打开、关闭和删除索引文件 | 否 |
| `b_plus_tree.h/.cpp` | 页内操作、B+ 树查找、插入、删除和并发控制 | 是 |
| `index_scan.h/.cpp` | 沿叶子链表遍历索引项 | 否 |

建议按 `index_types` → `index_manager` → `BPlusTreeNode` → `BPlusTree` → `IndexScan` 的顺序阅读。

## 页面布局

索引文件的前三页由框架预留：

| 页号 | 内容 |
| --- | --- |
| 0 | `IndexFileHeader` 的序列化数据 |
| 1 | 叶子链表哨兵页 |
| 2 | 初始根页，也是空树的唯一叶子页 |

普通节点页依次存放 `IndexPageHeader`、定长键数组和 `Rid` 数组。键与 `Rid` 使用相同下标一一对应；内部节点的 `Rid::page_no` 表示孩子页号，叶节点的 `Rid` 表示表记录位置。

## 对象关系

- `IndexManager` 负责索引文件生命周期，不实现 B+ 树算法。
- `BPlusTree` 对应一个已打开的索引文件，持有内存中的文件头副本。
- `BPlusTreeNode` 只是缓冲池页面的临时视图，不拥有页面；获取节点后必须正确解固定页面。
- `IndexScan` 不拥有 B+ 树，只记录当前叶子页与槽号。
- Lab2 测试中的 `b_plus_tree_invariant_checker` 是只读诊断工具，不代替学生实现查找、插入或删除算法。

具体实验要求和接口说明见 [Lab 2：索引管理](<../../docs/RUCBase-Lab2[索引管理].md>)。
