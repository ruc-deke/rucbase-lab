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

Rucbase查询执行模块采用的是火山模型(Volcano Model),你可以通过[链接](https://www.computer.org/csdl/journal/tk/1994/01/k0120/13rRUwI5TRe)获取相应论文阅读以理解火山模型的基本概念。你也可以阅读Rucbase在设计实现时的[相关文档](https://share.weiyun.com/mkXFy4bS)，在文档中给出了一个SQL语句

```sql
SELECT R.id , S.date 
FROM R JOIN S 
ON R.id = S.id 
WHERE S.value>100;
```

在Rucbase项目中的处理流程。



在本实验中，你需要实现Rucbase所支持的扫描，更新，Loop Join，删除和投影算子的相应`std::unique_ptr<RmRecord> Next()`

接口的实现，并对部分数据操纵语言的管理器接口进行完善。

为降低上手难度，读表，输出等操作逻辑已经实现，你只需要实现最核心逻辑即可，需要实现的部分代码段以

```c
// lab3 taskNo. Todo
//...
// lab3 taskNo. Todo End
```

给出

由于本实验关系到系统核心功能，与多个模块有所关联，实验同时提供**查询执行实验指导**文档，在文档中会对部分实验任务进行相应的解析，你可以阅读以理解如何调用系统其他模块的接口。

本实验暂不考虑事务和并发问题。

**代码目录**:

```
rucbase/src/execution
rucbase/src/system
rucbase/src/record
rucbase/src/index
```

**本实验时间安排：25days~35days**

## 任务一：元数据管理和DDL实现 (5-7Days)

> 本任务主要让学生了解数据库系统基本的元数据组织与管理的实现细节，包括理解rucbase中数据库表，表中字段和数据类型的结构。并借助*nix系统调用完成数据库系统的文件访问修改操作。并对部分DDL语句进行实现为后续DML,DQL语句的实现的工具类做好准备。
>
> 考察点：数据库库表元数据组织，文件操作，DDL建表删表实现逻辑

**代码目录**

```
src/system/sm_manager.h|.cpp
src/system/sm_meta.h
```

### 元数据管理 (1-2Days)

在`sm_meta.h`中，DbMeta维护了数据库相关元数据，包括数据库的名称和数据库中创建的表，数据结构如下：

```c++
class DbMeta {
  private:
    std::string name_;    // 数据库名称
    std::map<std::string, TabMeta> tabs_;   // 数据库内的表名称和元数据的映射
};
```

表的元数据和字段的元数据分别用TabMeta和ColMeta来维护：

```c++
struct TabMeta {
    std::string name;   // 表的名称
    std::vector<ColMeta> cols;    // 表的字段
    // when the transaction is running, just make the delete_mark_ = true to drop the table
    // when the transaction is committing, the system will delete the table
    bool delete_mark_{false};
};

struct ColMeta {
    std::string tab_name;   // 字段所属表名称
    std::string name;       // 字段名称
    ColType type;           // 字段类型
    int len;                // 字段长度
    int offset;             // 字段位于记录中的偏移量
    bool index;             // 该字段上是否建立索引
};
```

ColMeta中字段的类型包括int类型(`TYPE_INT`)、float类型(`TYPE_FLOAT`)和string类型(`TYPE_STRING`)。

在`execution_manager.h`中，给出了表列(TabCol)，列值(Value)以及条件子句(Condition, SetClause)结构的具体定义, QlManager则负责了DQL和DML语句的执行。

- 你需要实现`sm_meta.h`中结构体`TabMeta`中的方法

```cpp
    /**
     * @brief 根据列名在本表元数据结构体中查找是否有该名字的列
     *
     * @param col_name 目标列名
     * @return true
     * @return false
     */
    bool is_col(const std::string &col_name);


    /**
     * @brief 根据列名获得列元数据ColMeta
     *
     * @param col_name 目标列名
     * @return std::vector<ColMeta>::iterator
     */
    std::vector<ColMeta>::iterator get_col(const std::string &col_name) ;
```

- 你需要实现`DbMeta`中的方法

```cpp
    /**
     * @brief Get the table object
     *
     * @param tab_name 目标表名
     * @return TabMeta&
     */
    TabMeta &get_table(const std::string &tab_name)
```

注意，为了便于后续实验，你需要在本方法中考虑表是否在本事务中删除，如果删除你不应该继续获取本表`TabMeta`

是否删除可以查看字段`delete_mark_`

### DDL语句实现* (2-3Days)

在`sm_manager`中，系统给出了常见DDL语句`CREATE, DROP`和功能语句`SHOW, DESC`等的实现，你需要补全以下方法

```cpp
void SmManager::create_db(const std::string &db_name);

void SmManager::close_db();

void SmManager::drop_table(const std::string &tab_name, Context *context);	
```

注意，这些方法的操作可能涉及到系统`cmd`指令，你可以使用`system()`调用操作系统相应方法

你可以查看这些方法对应的反向方法的具体实现，这会对你的实验提供一定的参考。你也可以查看**查询执行实验指导**文档中的相应提示。

**注意！！！**

尽管在`system`模块中，并未要求你实现大量的方法接口，你**仍然应该**仔细阅读没有要求的方法具体实现和已定义的结构或类，这些将在你后续任务中使用。

### 任务完成功能

本任务完成后，你的系统将可以运行以下语句示例展示的功能

```sql
create table tb(s int, a int, b float, c char(16));
create table tb2(x int, y float, z char(16), s int);
create table tb3(m int, n int);
create table tb4(k int, y int);
create index tb(s);
desc tb;
show tb;
drop table tb4;
```





## 任务二：DQL—— select_from语句和相关算子实现(10-15Days)

> 在本任务中，你需要实现`rucbase`最核心的功能——查询语句执行的实现。首先，我们需要实现单表扫描的`scanExecutor`算子和连接`loopJoinExecutor`算子以及投影`ProjectionExecutor`算子。
>
> 考察点：数据库火山执行模型，B+树在数据库系统中应用，算子树构造，连接运算优化

###  算子实现

在补充实现算子时，你应该先查看每个算子类的成员和**构造函数**。在构造函数中会对部分类成员进行初始化，每个算子在构造函数中初始化或赋值的成员在这个算子实现功能时将发挥作用。

如在数据操作时的成员：

```cpp
std::unique_ptr<RmFileHandle>> fhs_;
std::unique_ptr<IxIndexHandle>> ihs_;
```

两种句柄分别提供了记录文件和索引文件的操作方法，这些是需要你自行阅读使用的。文档也会给出部分示例，但是不能仅依靠文档的说明。
关于表元组数据记录的相关操作接口，你需要阅读

```
src/record/rm_file_handle.h
src/record/rm_file_handle.cpp
src/record/rm_scan.h
src/record/rm_scan.cpp
src/record/rm_manager.h
```

关于索引数据的相关操作接口，你需要参考之前的索引实验，有关代码文件为

```
src/index/ix_scan.h
src/index/ix_scan.cpp
src/index/ix_index_handle.h
src/index/ix_index_handle.cpp
src/index/ix_manager.h
```

**代码目录**

```
src/execution/executor_index_scan.h
src/execution/executor_seq_scan.h
src/execution/executor_nestedloop_join.h
src/execution/executor_projection.h
```

所有算子都继承了抽象算子类`execuotr_abstract`，它给出了各个算子继承的基类抽象算子的声明和相应方法。

#### 扫描算子(顺序* & 索引) (2-3Days)

在扫描算子中，你需要实现4个方法：

```cpp
    /**
     * @brief 构建表迭代器scan_,并开始迭代扫描,直到扫描到第一个满足谓词条件的元组停止,并赋值给rid_
     *
     */
    void beginTuple() override

    /**
     * @brief 从当前scan_指向的记录开始迭代扫描,直到扫描到第一个满足谓词条件的元组停止,并赋值给rid_
     *
     */
    void nextTuple() override
        
    /**
     * @brief 返回下一个满足扫描条件的记录
     *
     * @return std::unique_ptr<RmRecord>
     */
    std::unique_ptr<RmRecord> Next() override 

	/**
     * @brief 将连接运算的条件谓词右值变量更新为实际值,更新实际的fed_conds_,建议在连接算子任务中实现
     *
     * @param feed_dict
     */
    void feed(const std::map<TabCol, Value> &feed_dict)
```

每当系统`call scanExecutor.Next()`时，都会获得一个符合谓词条件的`RmRecord`  ，而`beginTuple, nextTuple`则在移动算子持有的`scan_`时发挥作用。

```cpp
	std::string tab_name_;
    std::vector<Condition> conds_;  // 初始扫描条件(来自SQL)
    RmFileHandle *fh_;              // TableHeap
    std::vector<ColMeta> cols_;
    size_t len_;
    std::vector<Condition> fed_conds_;  // 实际扫描条件(可能由于连接运算动态改变)

    Rid rid_;                        // 当前扫描到的记录的rid,Next()返回该rid对应的records
    std::unique_ptr<RecScan> scan_;  // table_iterator

```

其中`scan_`是最重要的成员之一，它在`beginTuple()`中给出了实例生成，其提供了对表元祖的迭代扫描操作，你可以在`src/record/rm_scan.cpp`中查看。

注意扫描算子实际的扫描条件是`fed_conds_`

对于索引算子，为了生成`scan_`，你需要根据条件谓词对给定的IndexID变量

```cpp
Iid lower = ih->leaf_begin();
Iid upper = ih->leaf_end();
```

进行调整，系统将根据你调整的IndexID构造`scan_`

#### 连接算子(Nested Loop Join)* (2-3Days)

多表连接语句的语法结构是这样的：

```sql
select [col..] from TbName join TbName ... where cond; 
```

连接算子不能够作为算子树的叶子节点，它的结构中有两个指向左右孩子算子的指针

```cpp
std::unique_ptr<AbstractExecutor> left_;
std::unique_ptr<AbstractExecutor> right_;
```

**在本系统中，默认规定用的连接方式是连接算子作为右孩子，你需要以此进行理解**

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

此外，你应该阅读`feed()和feed_right()`方法，理解如何将连接sql语句中的`cond`根据当前的连接状态进行转换。

关于`nested loop join`内表(innter Table)和外表(outer Table)的概念，你可以以下面的伪代码进行理解

```cpp
for each row R1 in the outer table
    for each row R2 in the inner table
        if R1 joins with R2
            return (R1, R2)
```

我们以SQL语句

```sql
select * from 765pro, 346pro where 765pro.id<346pro.id
```

为例，表`346pro`作为`innerTable`，`765pro`作为`outerTable`，在SQL解析后生成扫描算子时，两个表的初始扫描条件` std::vector<Condition> conds_`是`765pro.id<346pro.id`，当`outerTable`目前扫描到记录`{id:2, name='ranko'}`时，`innerTable`的扫描条件就可以从原来的`765pro.id<346pro.id`转换为`765pro.id<2`。也就是内表的扫描条件右值在当前外表的Tuple确定时可以转换为确定的值。

由于连接算子在当前规定下左算子必为扫描算子，因此`LoopJoinExecutor.feed()`中会调用`left_->feed()`将innerTable算子的谓词条件从SQL语句`A.id>B.id`更新成当前outerTable确定的`BTuple val`,这样条件谓词就会成为`A.id > {BTuple val}`

理解这些后，你可以补全扫描算子的`feed()`了

#### 投影算子 (1-2Days)

投影算子调用其要处理的`prev`算子的`Next()`获得元祖，根据要投影的列进行投影返回投影后的新元祖。你需要补全方法

```cpp
std::unique_ptr<RmRecord> Next() override{}
```

方法已经给出了获取prevs算子的下一个元组和一个空的返回记录

```cpp
auto prev_rec = prev_->Next();
auto proj_rec = std::make_unique<RmRecord>(len_);
```

你需要根据构造方法中初始化的`std::vector<size_t> sel_idxs_`选择合适的列对`proj_rec`进行写入，以符合返回值要求。

#### select_from语句补全 (3Days)

实现了扫描算子，接下来你需要补全select_from语句以实现算子树的构建。

```cpp
	/**
     * @brief 查询操作
     *
     * @param sel_cols 选取的列向量数组
     * @param tab_names 目标表名数组
     * @param conds 条件谓词
     * @param context 执行上下文, 用于事务lab, 暂时不考虑
     */
    void select_from(std::vector<TabCol> sel_cols, const std::vector<std::string> &tab_names,std::vector<Condition> conds, Context *context);
```

你需要根据是否存在索引实现生成不同的算子。如果存在多表,你还需要生成LoopJoin Executor并把算子组合成一颗二叉树，并生成根投影节点算子。**请先按照文档中的设计默认规定，将叶子扫描算子作为左子节点，连接算子作为右子节点**

同样以`select * from A,B,C`为例，你的处理逻辑可以如下：

```cpp
//make scanExecutor for table A,B,C
//make loopjoinExecutor L1  with B,C 
//    L1
//   /  \
//  B    C
//make loopjoinExecutor L2  with A,L1
//          L2
//         /  \
//        A    L1
//           /    \
//          B       C
//make projExecutor P with L2
//          P
//          |
//          L2
//         /  \
//        A    L1
//           /    \
//          B       C
```
##### 可选任务(5-7Days) 100 Points

现在系统的`select`处理构建的二叉算子树是向右生长的，左孩子都是叶子扫描算子，因此连接算子使用的是`feed_right`，让左孩子(扫描叶子算子，outerTable)调用`feed()`修改自己的谓词条件和innterTable的当前元组匹配。

你是否可以尝试修改这些逻辑，让算子二叉树**向左生长，或者让其成为一颗平衡二叉树？**

### 任务完成功能

本任务完成后，你的系统将可以运行以下语句示例展示的功能

```sql
select * from tb;
select s,a from tb where s>10 and a<=5;
select x,s from tb join tb2 where tb.s=tb2.s;
select * from tb,tb2,tb3;
```



## 任务三：DML—— INSERT/DELETE/UPDATE语句和算子实现(10-15Days)

> 在本任务中，你需要分别实现`rucbase`的DML语句功能，包括基本的增删改。
>
> 考察点：数据库火山执行模型，B+树在数据库系统中应用

### Insert 插入操作的实现(5Days)

insert语句的语法结构是这样的：

```sql
insert into table_name values (?,?,?,?);
```

插入操作基本思路是根据给定的`std::vector<Value> values`生成合适的`RmRecord`，使用算子的`RmFileHandle`提供的`record`相关方法在记录文件中插入新的记录，并对有索引的列进行索引文件更新，索引文件的更新使用`IxIndexHandle`。

#### 插入算子

在插入算子中，你需要补充完成方法

```cpp
std::unique_ptr<RmRecord> Next() override{}
```

请注意recordFile和indexFile都可能需要更新

#### insert_into()语句补全

你需要完成`execution_manager.QlManager`中的方法

```cpp
void QlManager::insert_into(const std::string &tab_name, std::vector<Value> values, Context *context) {}
```

你只需要根据方法参数构建一个符合要求的`InsertExecutor`, 并调用`InsertExecutor.Next()`即可

#### 任务完成功能

本任务完成后，你的系统将可以运行以下语句示例展示的功能

```sql
insert into tb values (0, 1, 1.2, 'abc');
insert into tb values (2, 2, 2.0, 'def');
insert into tb values (5, 3, 2., 'xyz');
```

### Delete 删除操作的实现(5Days)

delete语句的语法结构是这样的：

```sql
delete from tab_name where cond;
```

#### 删除算子

在删除算子中，你需要补充完成方法

```cpp
std::unique_ptr<RmRecord> Next() override {};
```

该方法需要把算子成员(由构造函数赋值)

```cpp
std::vector<Rid> rids_;
```

标记的所有id的记录删除。

删除算子操作的思路可以参考插入操作的逆向，构造方法中的`std::vector<Rid> rids`是根据扫描算子得到的应该删除的记录组，你可以先获取`IxIndexHandle`删除对应的索引`entry`，再获取`RmFileHandle`删除记录。

#### delete_from语句补全

你需要补全`execution_manager.QlManager`中的方法

```cpp
void QlManager::delete_from(const std::string &tab_name, std::vector<Condition> conds, Context *context) 
```

你需要根据`tab_name,conds`生成扫描算子，获取要删除的记录id集合`std::vector<Rid> rids`，然后以此构造`executor_delete`，并调用`Next()`接口完成删除操作。

#### 任务完成功能

本任务完成后，你的系统将可以运行以下语句示例展示的功能

```sql
delete from tb where a = 996;
```

### Update 更新操作的实现(5Days)

update语句的语法结构是这样的：

```sql
update project_member set name ="Chihaya" where id=72;
```

#### 更新算子

在更新算子中，你需要补充完成方法：

```cpp
std::unique_ptr<RmRecord> Next() override {}
```

该方法需要把算子成员(由构造函数赋值)

```cpp
std::vector<Rid> rids_;
```

标记的所有id的记录进行修改（与删除算子行为相似）。

更新算子操作的思路可以参考删除操作，构造方法中的`std::vector<Rid> rids`是根据扫描算子得到的应该更新的记录组，根据已有代码提示，你需要先获取在更新值时所有关联的索引句柄收集在

```cpp
std::vector<IxIndexHandle *> ihs(tab_.cols.size(), nullptr);
```

结构中。

再遍历`rids_`，利用`ihs`进行以下顺序步骤：

1. 删除旧索引entry   （通过`RmFileHandle->get_record()`和  `IxIndexHandle->delete_entry( ... )  `）
2. 更新record file （通过和`RmFileHandle->update_record`）
3. 插入新的索引entry（通过`IxIndexHandle->insert_entry( ... )  `）

#### update_set 语句补全

你需要补全`execution_manager.QlManager`中的方法

```cpp
void QlManager::update_set(const std::string &tab_name, std::vector<SetClause> set_clauses,std::vector<Condition> conds, Context *context)
```

你需要根据`std::vector<Condition> conds`更新条件谓词（通过`check_where_clause()`），并从set从句集合`set_clauses`中获取要更新的值（如sql例子中set从句就是`name="Chihaya"`，你要从中获取字符串类型值`Chihaya`以用于更新Tuple）

此外，你还要构造扫描算子（可参考前几个任务）并扫描目标表，获取更新操作目标记录id集合`std::vector<Rid> rids`，并以此构建`executor_insert`，并调用`Next()`接口完成更新操作。

#### 任务完成功能

本任务完成后，你的系统将可以运行以下语句示例展示的功能

```sql
update tb set b = 997., c = 'icu' where c = 'xyz';
```

至此，你已经得到了一个可执行简单增删改查SQL，有输出反馈，单用户非并发的简单数据库系统原型。你将在最后的lab中对这个原型进行升级，添加基本的事物并发和日志支持


## 分数说明

本实验共设置4个检查点脚本，每个检查点赋分如下：

- `task1_test.sh`：20 Points
- `task2_test.sh`：30 Points
- `task3_test.sh`：30 Points
- `taskall_test.sh`：20 Points

可选任务分值为100 Points，如果你完成了可选任务，你需要另附实现代码并在实验报告中给出必要的说明。

可选任务有效给分基本要求是通过`task2_test.sh`

