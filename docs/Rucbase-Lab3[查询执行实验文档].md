<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->

- [查询执行文档](#%E6%9F%A5%E8%AF%A2%E6%89%A7%E8%A1%8C%E6%96%87%E6%A1%A3)
  - [任务一：元数据管理和DDL实现 (5-7Days)](#%E4%BB%BB%E5%8A%A1%E4%B8%80%E5%85%83%E6%95%B0%E6%8D%AE%E7%AE%A1%E7%90%86%E5%92%8Cddl%E5%AE%9E%E7%8E%B0-5-7days)
    - [元数据管理 (1-2Days)](#%E5%85%83%E6%95%B0%E6%8D%AE%E7%AE%A1%E7%90%86-1-2days)
    - [DDL语句实现* (2-3Days)](#ddl%E8%AF%AD%E5%8F%A5%E5%AE%9E%E7%8E%B0-2-3days)
    - [任务完成功能](#%E4%BB%BB%E5%8A%A1%E5%AE%8C%E6%88%90%E5%8A%9F%E8%83%BD)
  - [任务二：DQL—— select_from语句和相关算子实现(10-15Days)](#%E4%BB%BB%E5%8A%A1%E4%BA%8Cdql-select_from%E8%AF%AD%E5%8F%A5%E5%92%8C%E7%9B%B8%E5%85%B3%E7%AE%97%E5%AD%90%E5%AE%9E%E7%8E%B010-15days)
    - [算子实现](#%E7%AE%97%E5%AD%90%E5%AE%9E%E7%8E%B0)
      - [扫描算子(顺序* & 索引) (2-3Days)](#%E6%89%AB%E6%8F%8F%E7%AE%97%E5%AD%90%E9%A1%BA%E5%BA%8F--%E7%B4%A2%E5%BC%95-2-3days)
      - [连接算子(Nested Loop Join)* (2-3Days)](#%E8%BF%9E%E6%8E%A5%E7%AE%97%E5%AD%90nested-loop-join-2-3days)
      - [投影算子 (1-2Days)](#%E6%8A%95%E5%BD%B1%E7%AE%97%E5%AD%90-1-2days)
      - [select_from语句补全 (3Days)](#select_from%E8%AF%AD%E5%8F%A5%E8%A1%A5%E5%85%A8-3days)
        - [可选任务(5-7Days)](#%E5%8F%AF%E9%80%89%E4%BB%BB%E5%8A%A15-7days)
    - [任务完成功能](#%E4%BB%BB%E5%8A%A1%E5%AE%8C%E6%88%90%E5%8A%9F%E8%83%BD-1)
  - [任务三：DML—— INSERT/DELETE/UPDATE语句和算子实现(10-15Days)](#%E4%BB%BB%E5%8A%A1%E4%B8%89dml-insertdeleteupdate%E8%AF%AD%E5%8F%A5%E5%92%8C%E7%AE%97%E5%AD%90%E5%AE%9E%E7%8E%B010-15days)
    - [Insert 插入操作的实现(5Days)](#insert-%E6%8F%92%E5%85%A5%E6%93%8D%E4%BD%9C%E7%9A%84%E5%AE%9E%E7%8E%B05days)
      - [插入算子](#%E6%8F%92%E5%85%A5%E7%AE%97%E5%AD%90)
      - [insert_into()语句补全](#insert_into%E8%AF%AD%E5%8F%A5%E8%A1%A5%E5%85%A8)
      - [任务完成功能](#%E4%BB%BB%E5%8A%A1%E5%AE%8C%E6%88%90%E5%8A%9F%E8%83%BD-2)
    - [Delete 删除操作的实现(5Days)](#delete-%E5%88%A0%E9%99%A4%E6%93%8D%E4%BD%9C%E7%9A%84%E5%AE%9E%E7%8E%B05days)
      - [删除算子](#%E5%88%A0%E9%99%A4%E7%AE%97%E5%AD%90)
      - [delete_from语句补全](#delete_from%E8%AF%AD%E5%8F%A5%E8%A1%A5%E5%85%A8)
      - [任务完成功能](#%E4%BB%BB%E5%8A%A1%E5%AE%8C%E6%88%90%E5%8A%9F%E8%83%BD-3)
    - [Update 更新操作的实现(5Days)](#update-%E6%9B%B4%E6%96%B0%E6%93%8D%E4%BD%9C%E7%9A%84%E5%AE%9E%E7%8E%B05days)
      - [更新算子](#%E6%9B%B4%E6%96%B0%E7%AE%97%E5%AD%90)
      - [update_set 语句补全](#update_set-%E8%AF%AD%E5%8F%A5%E8%A1%A5%E5%85%A8)
      - [任务完成功能](#%E4%BB%BB%E5%8A%A1%E5%AE%8C%E6%88%90%E5%8A%9F%E8%83%BD-4)
  - [分数说明](#%E5%88%86%E6%95%B0%E8%AF%B4%E6%98%8E)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# 查询执行文档

Rucbase查询执行模块采用的是火山模型(Volcano Model),你可以通过[链接](https://www.computer.org/csdl/journal/tk/1994/01/k0120/13rRUwI5TRe)获取相应论文阅读以理解火山模型的基本概念，并结合项目结构文档理解系统查询模块的整体架构和处理流程。

## 实验一：元数据管理和DDL语句 (25分)

在本实验中，你需要完成src/system/sm_manager.cpp中的接口，使得系统能够支持DDL语句，具体包括create table、drop table、create index和drop index语句。

相关代码位于`src/system/sm_manager.h`和`src/system/sm_manager.cpp`文件中。在`src/system/sm_manager.cpp`文件中，给出了`create_table`、`create_db`和`drop_db`函数的实现示例，你需要参照这些函数实现剩余的空缺函数。

在`SmManager`类中，你需要实现的函数接口如下：
- `open_db(...)`：系统通过调用该接口打开数据库

在运行系统时，你需要在命令行中输入一个参数\<database name\>，代表当前数据库的名称，系统会创建一个同名文件夹，并在文件夹中存储所有数据库相关的数据文件。系统再次启动时，会打开数据库名称的同名文件夹，并通过读取元数据文件获取当前数据库的相关元数据。
同时，系统还需要打开所有的表文件和索引文件，并将文件句柄加载到系统中。

- `close_db(...)`：系统通过调用该接口关闭数据库

在系统服务端关闭时，系统调用该函数，将元数据信息、表数据等落盘，并关闭相关文件。

- `drop_table(...)`：删除表

系统调用该函数删除指定名称等表，并删除相关的数据文件，更新元数据信息

- `create_index(...)`：创建索引

系统调用该函数在指定表的指定属性上创建索引（该函数在lab2中应该已经实现了）

- `drop_index(...)`：删除索引

系统调用该函数删除指定表的指定索引（该函数在lab2中应该已经实现了）

### 测试点及分数

在完成本任务后，你需要使用src/test文件夹下的query_unit_test.py单元测试文件来进行basic_query_test1.sql的测试

```bash
cd src/test/query
python query_unit_test.py basic_query_test1.sql # 25分
python query_unit_test.py basic_query_test{i}.sql  # replace {i} with the desired test file index
```


## 实验二：DML语句实现（75分）

在本实验中，你需要完成src/execution文件夹下执行算子中的空缺函数，使得系统能够支持增删改查四种DML语句。

相关代码位于`src/execution`文件夹下，其中需要完成的文件包括`executor_delete.h`、`executor_nestedloop_join.h`、`executor_projection.h`、`executor_seq_scan.h`、`executor_update.h`，已经实现的文件包括`executor_insert.h`和`execution_manager.cpp`。

在本实验中，你需要仿照insert算子实现如下算子：

- `SeqScan`算子：你需要实现该算子的`Next()`、`beginTuple()`和`nextTuple()`接口，用于表的扫描，你需要调用`RmScan`中的相关函数辅助实现（这些接口在lab1中已经实现）；
- `Projection`算子：你需要实现该算子的`Next()`、`beginTuple()`和`nextTuple()`接口，该算子用于投影操作的实现；
- `NestedLoopJoin`算子：你需要实现该算子的`Next()`、`beginTuple()`和`nextTuple()`接口，该算子用于连接操作，在本实验中，你只需要支持两表连接；
- `Delete`算子：你需要实现该算子的`Next()`接口，该算子用于删除操作；
- `Update`算子：你需要实现该算子的`Next()`接口，该算子用于更新操作。

所有算子都继承了抽象算子类`execuotr_abstract`，它给出了各个算子继承的基类抽象算子的声明和相应方法。


### 连接算子实现提示

多表连接语句的语法如下：

```sql
select [col..] from TbName join TbName ... where cond; 
```

连接算子不能够作为算子树的叶子节点，它的结构中有两个指向左右孩子算子的指针

```cpp
std::unique_ptr<AbstractExecutor> left_;
std::unique_ptr<AbstractExecutor> right_;
```

**在本系统中，默认规定用的连接方式是连接算子作为右孩子**

如，`select * from A,B,C`，系统生成的算子树如下

```cpp
//          P (Projection)
//          |
//          L2  (LoopJoin)
//         /  \
//        A    L1
//           /    \
//          B      C  (C table scan)
```

你需要补充完成`LoopJoin`算子中的以下3个方法：

```cpp
void beginTuple() override {}

void nextTuple() override {}

std::unique_ptr<RmRecord> Next() override{}
    
```

### 测试点及分数

在完成本实验所有任务后，你需要通过src/test文件夹下的basic_query_test1～basic_query_test5.sql测试。

```bash
cd src/test/query
python query_test_basic.py  # 100分
```


## 测试说明

本实验通过SQL语句黑盒测试，包括五个测试点，每个测试点测试内容和分数如下：

| **测试点**     | **测试内容**      | **分数**      |
| ------------- | ----------------- | ------------- |
| `basic_query_test1` | 表和索引的创建与删除  | 25 |
| `basic_query_test2` | 单表插入与条件查询    | 15 |
| `basic_query_test3` | 单表更新与条件查询    | 15 |
| `basic_query_test4` | 单表删除与条件查询    | 15 |
| `basic_query_test5` | 多表连接与条件查询    | 30 |

**注意⚠️：**

在本测试中，要求把select语句的输出写入到指定文件中，写入逻辑已经在select_from函数中给出，不要修改写入格式。
