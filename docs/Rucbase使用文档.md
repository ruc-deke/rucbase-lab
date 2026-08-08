# Rucbase使用指南

<!-- START doctoc generated TOC please keep comment here to allow auto update -->

<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->

<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->

- [环境配置](#%E7%8E%AF%E5%A2%83%E9%85%8D%E7%BD%AE)
- [项目下载](#%E9%A1%B9%E7%9B%AE%E4%B8%8B%E8%BD%BD)
  - [GoogleTest依赖说明](#googletest%E4%BE%9D%E8%B5%96%E8%AF%B4%E6%98%8E)
- [编译](#%E7%BC%96%E8%AF%91)
- [运行 (S/C)](#%E8%BF%90%E8%A1%8C-sc)
- [测试单元](#%E6%B5%8B%E8%AF%95%E5%8D%95%E5%85%83)
- [系统简介](#%E7%B3%BB%E7%BB%9F%E7%AE%80%E4%BB%8B)
  - [SQL Example](#sql-example)
- [基本结构](#%E5%9F%BA%E6%9C%AC%E7%BB%93%E6%9E%84)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

## 环境配置

Rucbase需要以下依赖环境库配置：

- 课程Docker镜像或手工安装均以Ubuntu 24.04 LTS为基线
- GCC 11及以上版本（推荐GCC 13），或Clang 14及以上版本
- CMake 3.22及以上版本
- flex
- bison

可以通过命令完成环境配置(以Debian/Ubuntu-apt为例)

```bash
sudo apt-get install build-essential  # build-essential packages, including gcc, g++, make and so on
sudo apt-get install cmake            # cmake package
sudo apt-get install flex bison       # flex & bison packages
sudo apt-get install python3-pytest   # Python black-box tests
```

可以通过`gcc --version`和`cmake --version`命令查看当前版本。Ubuntu 24.04软件源提供的默认GCC和CMake已经满足要求。

注意,在CentOS下,编译时可能存在头文件冲突的问题,我们不建议你使用Ubuntu以外的操作系统,你可以向助教获取帮助

## 项目下载

网络环境正常时可以直接克隆项目。配置阶段如果没有找到GoogleTest子模块或系统安装包，CMake会自动获取项目固定的GoogleTest版本：

```bash
git clone https://github.com/ruc-deke/rucbase-lab.git
```

如果需要提前准备完整依赖，或者之后将在离线环境中编译，请递归克隆子模块：

```bash
git clone --recursive https://github.com/ruc-deke/rucbase-lab.git
```

**注意，当新lab放出时，你需要先使用git pull命令拉取最新的实验文档**

### GoogleTest依赖说明

项目按照以下顺序查找GoogleTest：

1. 使用已经拉取的`deps/googletest`子模块；
2. 使用系统中已经安装的GoogleTest CMake包；
3. 通过CMake自动获取固定版本的GoogleTest。

不再需要手工编译并执行`sudo make install`。如果项目已经克隆但需要补充离线依赖，可以执行：

```bash
git submodule update --init --recursive
```

如果需要禁止配置阶段访问网络，请在已经准备好子模块或系统GoogleTest的前提下，增加`-DRUCBASE_FETCH_DEPENDENCIES=OFF`。

## 编译

整个系统分为服务端和客户端。推荐通过仓库提供的CMake Preset一键配置和编译：

```bash
# 配置（首次或 CMakeLists 变更后）
cmake --preset debug

# 只编服务端 + 官方客户端（日常开发推荐）
cmake --build --preset debug-client -j 4

# 或只编其中一个目标
cmake --build --preset debug --target rmdb -j 4
cmake --build --preset debug --target rucbase_client -j 4

# 编全部目标（含单元测试等）
cmake --build --preset debug -j 4
```

产物位于`build/debug/bin/`（例如`rmdb`、`rucbase_client`）。将`debug`替换为`release`可以构建发布版本；还可以分别使用`asan`、`ubsan`或`tsan`预设检查内存错误、未定义行为和数据竞争。不同预设分别输出到`build/<preset>`目录，互不污染。

不使用Preset时，仍可以执行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target rmdb --target rucbase_client -j 4
```

客户端由课程方提供，一般无需学生修改。若需单独在`rucbase_client/`目录下编译，仍可：

```bash
cmake -S rucbase_client -B rucbase_client/build -DCMAKE_BUILD_TYPE=Debug
cmake --build rucbase_client/build -j 4
```

## 运行 (S/C)

首先运行服务端：

```bash
cd build/debug
./bin/rmdb <database_name> # 如果存在该数据库,直接加载;若不存在该数据库,自动创建
# 自定义端口示例：./bin/rmdb -p 9000 <database_name>
# 默认仅监听 127.0.0.1；允许远程连接：./bin/rmdb -b 0.0.0.0 -p 9000 <database_name>
# 自定义 socket I/O 超时（秒）：./bin/rmdb -t 300 <database_name>
```

以上路径对应`debug` Preset。如果使用`cmake -S . -B build`手工配置，则进入`build`目录运行。

然后开启客户端，用户可以同时开启多个客户端：

```bash
cd build/debug
./bin/rucbase_client
# 连接指定主机/端口：./bin/rucbase_client -h 127.0.0.1 -p 9000
# 执行单条语句：./bin/rucbase_client -e "show tables;"
# 执行脚本文件：./bin/rucbase_client -f demo.sql
# 查看全部选项：./bin/rucbase_client --help
```

交互模式下支持多行输入（以`;`结束一条语句）和一次粘贴多条语句；字符串或注释中的`;`不会被当作语句结尾。用户可以通过`exit;` / `bye;` 或 Ctrl-D 关闭客户端：

```bash
Rucbase(<database_name>)> exit;
```

服务端的关闭需要在服务端运行界面使用ctrl+c来进行关闭，关闭服务端时，系统会把数据页刷新到磁盘中。

+ 如果需要删除数据库，则需要在build文件夹下删除与数据库同名的目录
+ 如果需要删除某个数据库中的表文件，则需要在build文件夹下找到数据库同名目录，进入该目录，然后删除表文件

## 测试单元

GoogleTest框架测试

包含以下模块测试：

- 存储模块：

  - disk_manager_test
  
  - lru_replacer_test
    
  - buffer_pool_manager_test
  
  - record_manager_test

- 索引模块：
  
  - b_plus_tree_insert_test
  - b_plus_tree_delete_test
  - b_plus_tree_concurrent_test

- 执行模块：
  
  - task1_test.sh
  - task2_test.sh
  - task3_test.sh
  - taskall_test.sh

- 事务模块：
  
  - txn_test
  - lock_test
  - concurrency_test

所有C++测试和Python黑盒测试均由CTest统一调度：

```bash
cmake --build --preset debug -j 4
ctest --preset debug
```

只运行GoogleTest和解析器单元测试，可以执行`ctest --preset debug -L unit`；只运行查询、事务和并发黑盒测试，可以执行`ctest --preset debug -L blackbox`。黑盒测试使用pytest启动服务端，每个测试使用独立的临时数据库目录和动态端口。测试失败时，服务端和客户端日志保存在`build/debug/test-logs`。

## 系统简介

### SQL Example

目前系统支持基础DML和DDL语句，包括以下语句：

- create/drop table;
- create/drop index;
- insert;
- delete;
- update;
- begin;
- commit/abort;

目前事务的并发控制暂时支持可重复读隔离级别，事务暂时只支持基础insert、delete、update和select操作。

以下为SQL操作demo，具体下SQL语法可以在系统中使用help语句查询：

```sql
create table student (id int, name char(32), major char(32));
create index student (id);
create table grade (course char(32), student_id int, score float);
create index grade (student_id);

show tables;
desc student;

begin;
insert into student values (1, 'Tom', 'Computer Science');
insert into student values (2, 'Jerry', 'Computer Science');
insert into student values (3, 'Jack', 'Electrical Engineering');
commit;

begin;
select * from student where id>=1;
update student set major = 'Electrical Engineering' where id = 2;
select * from student where id>=1;
delete from student where name = 'Jack';
select * from student where id>=1;
commit;

begin;
insert into grade values ('Data Structure', 1, 90.0);
insert into grade values ('Data Structure', 2, 95.0);
insert into grade values ('Calculus', 2, 82.0);
insert into grade values ('Calculus', 1, 88.5);
abort;

begin;
insert into grade values ('Data Structure', 1, 90.0);
insert into grade values ('Data Structure', 2, 95.0);
insert into grade values ('Calculus', 2, 82.0);
insert into grade values ('Calculus', 1, 88.5);
commit;

select * from student, grade;
select id, name, major, course, score from student, grade where student.id = grade.student_id;
select id, name, major, course, score from student join grade where student.id = grade.student_id;

drop index student (id);
desc student;

drop table student;
drop table grade;
show tables;

exit;
```
