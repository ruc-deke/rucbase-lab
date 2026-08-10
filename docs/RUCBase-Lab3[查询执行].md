# Lab 3：查询执行

开始实验前，请先按照 [RUCBase 使用文档](RUCBase使用文档.md) 完成构建和测试环境配置，并阅读 [RUCBase 项目结构](RUCBase项目结构.md) 了解查询路径。

RUCBase 查询执行模块采用火山模型（Volcano Model）。可以通过[论文](https://www.computer.org/csdl/journal/tk/1994/01/k0120/13rRUwI5TRe)了解其基本概念。

![Lab 3 查询执行实验流程图](pics/Lab3流程图.png)

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

在完成本任务后，可以使用 `src/test/query/query_unit_test.py` 单独运行一个查询测试：

```bash
cd src/test/query
python3 query_unit_test.py basic_query_test1.sql # 25分
```

上述兼容命令内部使用pytest运行测试，不再删除已有构建目录。也可以在仓库根目录执行`ctest --preset lab3-query`。


## 实验二：DML语句实现（75分）

在本实验中，你需要完成src/execution文件夹下执行算子中的空缺函数，使得系统能够支持增删改查四种DML语句。

相关代码位于`src/execution`文件夹下，其中需要完成的文件包括`executor_delete.h`、`executor_nestedloop_join.h`、`executor_projection.h`、`executor_seq_scan.h`、`executor_update.h`，已经实现的文件包括`executor_insert.h`和`execution_manager.cpp`。

在本实验中，你需要仿照insert算子实现如下算子：

- `SeqScan`算子：你需要实现该算子的`Next()`、`beginTuple()`和`nextTuple()`接口，用于表的扫描，你需要调用`RmScan`中的相关函数辅助实现（这些接口在lab1中已经实现）；
- `Projection`算子：你需要实现该算子的`Next()`、`beginTuple()`和`nextTuple()`接口，该算子用于投影操作的实现；
- `NestedLoopJoin`算子：你需要实现该算子的`Next()`、`beginTuple()`和`nextTuple()`接口，该算子用于连接操作，在本实验中，你只需要支持两表连接；
- `Delete`算子：你需要实现该算子的`Next()`接口，该算子用于删除操作；
- `Update`算子：你需要实现该算子的`Next()`接口，该算子用于更新操作。

所有算子都继承自抽象算子类 `AbstractExecutor`，基类声明了各执行器需要实现的统一接口。

**注意⚠️：**

**部分基类虚函数在子类中没有标记 `Todo`，但仍需根据接口契约完成，否则程序无法正确运行。**


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
python3 query_test_basic.py  # 100分
```

测试失败时可在`build/debug/test-logs`中查看服务端和客户端日志。


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

当前版本通过 Wire 客户端接收 `SELECT` 的类型化结果，测试客户端将格式化结果输出到
stdout，黑盒测试直接比较客户端输出。服务端不再生成或维护 `output.txt`；执行错误以
Wire `ERROR` 响应返回，测试客户端将其归一化为 `failure`。
