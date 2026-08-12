# Record 模块

`record` 在缓冲池之上管理表的定长记录，是 Lab1 中磁盘/缓冲池与后续执行器之间的边界。

## 模块职责

| 类型           | 职责                                      | Lab1 是否需要学生实现 |
|----------------|-------------------------------------------|-----------------------|
| `RmManager`    | 创建、打开、关闭和删除记录文件            | 否                    |
| `RmFileHandle` | 按 RID 读取以及插入、删除、更新记录       | 是                    |
| `RmPageHandle` | 把已固定的 `Page` 解释为记录页视图        | 否                    |
| `RmScan`       | 按页号、槽号顺序遍历已占用槽位            | 是                    |
| `RmRecord`     | 拥有一份从页面复制出的记录数据            | 否                    |

指定 RID 的 `insert_record` 是为后续恢复或回滚预留的扩展接口，不属于 Lab1 评分任务。`Context` 参数同样为后续事务、锁和日志实验保留；Lab1 可以传入空成员的上下文。

## 磁盘布局

第 0 页保存 `RmFileHdr`。数据页从第 1 页开始，布局如下：

```text
0                        Page::OFFSET_PAGE_HDR
+------------------------+-----------+----------+------------------+
| Page 公共元数据（LSN） | RmPageHdr | bitmap   | fixed-size slots |
+------------------------+-----------+----------+------------------+
```

`RmPageHandle` 的构造函数按这个顺序解释页面，`RmManager::create_file()` 根据同一布局计算每页最多能够容纳的记录数。代码直接展示这些计算，便于结合上图阅读。

## 必须保持的不变量

- `num_pages` 包含第 0 号文件头页，数据页范围为 `[RM_FIRST_RECORD_PAGE, num_pages)`。
- `bitmap_size == ceil(num_records_per_page / BITMAP_WIDTH)`。
- 页头、bitmap 和全部记录槽不能超过 `PAGE_SIZE`。
- `num_records` 等于 bitmap 中置位的有效槽位数，范围为 `[0, num_records_per_page]`。
- `first_free_page_no` 为 `RM_NO_PAGE`，或指向一个未满的数据页。
- 只有未满页面位于空闲页链表中，同一页面在链表中至多出现一次。

## 页面生命周期

`RmPageHandle` 只是视图，不拥有缓冲池 pin。通过 `fetch_page_handle()` 或 `create_page_handle()` 得到页面后，调用路径必须最终恰好 unpin 一次；修改页面时还要把本次 pin 标记为 dirty。实现每个函数后，可以逐条检查 fetch/new 与 unpin 是否成对出现。
