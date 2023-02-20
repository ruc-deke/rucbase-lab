# Rucbase并发控制实验指南

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->

- [声明](#%E5%A3%B0%E6%98%8E)
  - [修改一：](#%E4%BF%AE%E6%94%B9%E4%B8%80)
  - [修改二：](#%E4%BF%AE%E6%94%B9%E4%BA%8C)
- [实验一 事务管理器实验（30分）](#%E5%AE%9E%E9%AA%8C%E4%B8%80-%E4%BA%8B%E5%8A%A1%E7%AE%A1%E7%90%86%E5%99%A8%E5%AE%9E%E9%AA%8C30%E5%88%86)
  - [测试点及分数](#%E6%B5%8B%E8%AF%95%E7%82%B9%E5%8F%8A%E5%88%86%E6%95%B0)
- [实验二 锁管理器实验（40分）](#%E5%AE%9E%E9%AA%8C%E4%BA%8C-%E9%94%81%E7%AE%A1%E7%90%86%E5%99%A8%E5%AE%9E%E9%AA%8C40%E5%88%86)
  - [任务：加锁解锁操作](#%E4%BB%BB%E5%8A%A1%E5%8A%A0%E9%94%81%E8%A7%A3%E9%94%81%E6%93%8D%E4%BD%9C)
    - [（1）行级锁加锁](#1%E8%A1%8C%E7%BA%A7%E9%94%81%E5%8A%A0%E9%94%81)
    - [（2）表级锁加锁](#2%E8%A1%A8%E7%BA%A7%E9%94%81%E5%8A%A0%E9%94%81)
    - [（3）解锁](#3%E8%A7%A3%E9%94%81)
  - [测试点及分数](#%E6%B5%8B%E8%AF%95%E7%82%B9%E5%8F%8A%E5%88%86%E6%95%B0-1)
- [实验三 并发控制实验（30分）](#%E5%AE%9E%E9%AA%8C%E4%B8%89-%E5%B9%B6%E5%8F%91%E6%8E%A7%E5%88%B6%E5%AE%9E%E9%AA%8C30%E5%88%86)
  - [测试点及分数](#%E6%B5%8B%E8%AF%95%E7%82%B9%E5%8F%8A%E5%88%86%E6%95%B0-2)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

Rucbase并发控制模块采用的是基于封锁的并发控制协议，要求事务达到可串行化隔离级别。在本实验中，你需要实现事务管理器、锁管理器，并使用事务管理器和锁管理器提供的相关接口保证事务正确地并发执行。

## 声明

本实验修改了如下两部分代码：

### 修改一：

本实验修改了**src/execution/executor_seq_scan.h**中的代码，在pull代码之后，首先需要检查该文件中的代码是否修改成功，如果未成功，需要手动进行修改，修改如下：

原代码：

```c++
auto rec = fh_->get_record(rid_, context_);  // TableHeap->GetTuple() 当前扫描到的记录
// lab3 task2 todo
// 利用eval_conds判断是否当前记录(rec.get())满足谓词条件
// 满足则中止循环
// lab3 task2 todo end
```

修改后的代码：

```c++
try {
    auto rec = fh_->get_record(rid_, context_);  // TableHeap->GetTuple() 当前扫描到的记录
    // lab3 task2 todo
    // 利用eval_conds判断是否当前记录(rec.get())满足谓词条件
    // 满足则中止循环
    // lab3 task2 todo end
} catch (RecordNotFoundError &e) {
    std::cerr << e.what() << std::endl;
}
```

请注意try代码块的范围。

### 修改二：

本实验修改了src/transaction中的代码，如果merge时出现冲突，可以直接强制覆盖，以最新版本为准。

## 实验一 事务管理器实验（30分）

在本实验中，你需要实现系统中的事务管理器，即`TransactionManager`类。

相关数据结构包括`Transaction`类、`WriteRecord`类等，分别位于`txn_def.h`和`transaction.h`文件中。

本实验要求完成事务管理器中的三个接口：事务的开始、提交和终止方法。

`TransactionManager`类的接口和重要成员变量如下：

```cpp
class TransactionManager{
public:
    static std::unordered_map<txn_id_t, Transaction *> txn_map;
    Transaction *GetTransaction(txn_id_t txn_id);

    Transaction * Begin(Transaction * txn, LogManager *log_manager);
    void Commit(Transaction * txn, LogManager *log_manager);
    void Abort(Transaction * txn, LogManager *log_manager);
};
```

其中，本实验提供以下辅助接口/成员变量：

- 静态成员变量`txn_map`：维护全局事务映射表

- `Transaction *GetTransaction(txn_id_t txn_id);`
  
  根据事务ID获取事务指针

你需要完成`src/transaction/transaction_manager.cpp`文件中的以下接口：

- `Begin(Transaction*, LogManager*)`：该接口提供事务的开始方法。
  
  **提示**：如果是新事务，需要创建一个`Transaction`对象，并把该对象的指针加入到全局事务表中。

- `Commit(Transaction*, LogManager*)`：该接口提供事务的提交方法。
  
  **提示**：如果并发控制算法需要申请和释放锁，那么你需要在提交阶段完成锁的释放。

- `Abort(Transaction*, LogManager*)`：该接口提供事务的终止方法。
  
  在事务的终止方法中，你需要对需要对事务的所有写操作进行撤销，事务的写操作都存储在Transaction类的write_set_中，因此，你可以通过修改存储层或执行层的相关代码来维护write_set_，并在终止阶段遍历write_set_，撤销所有的写操作。
  
  **提示**：需要对事务的所有写操作进行撤销，如果并发控制算法需要申请和释放锁，那么你需要在终止阶段完成锁的释放。
  
  **思考**：在回滚删除操作的时候，是否必须插入到record的原位置，如果没有插入到原位置，会存在哪些问题？

### 测试点及分数

```bash
cd build
make txn_manager_test  # 30分
./bin/txn_manager_test
```

## 实验二 锁管理器实验（40分）

在本实验中，你需要实现锁管理器，即`Lockanager`类。

相关数据结构包括`LockDataId`、`TransactionAbortException`、`LockRequest`、`LockRequestQueue`等，位于`txn_def.h`和`Lockanager.h`文件中。

本实验要求完成锁管理器`LockManager`类。

`LockManager`类的接口和重要成员变量如下：

```cpp
class LockManager {
public:
    // 行级锁
    bool LockSharedOnRecord(Transaction *txn, const Rid &rid, int tab_fd);
    bool LockExclusiveOnRecord(Transaction *txn, const Rid &rid, int tab_fd);
    // 表级锁
    bool LockSharedOnTable(Transaction *txn, int tab_fd);
    bool LockExclusiveOnTable(Transaction *txn, int tab_fd);
    // 意向锁
    bool LockISOnTable(Transaction *txn, int tab_fd);
    bool LockIXOnTable(Transaction *txn, int tab_fd);
    // 解锁
    bool Unlock(Transaction *txn, LockDataId lock_data_id);
private:
    std::unordered_map<LockDataId, LockRequestQueue> lock_table_;
};
```

其中，本实验提供辅助成员变量：

- `lock_table`：锁表，维护系统当前状态下的所有锁

在本实验中，你需要完成锁管理器的加锁、解锁和死锁预防功能。锁管理器提供了行级读写锁、表级读写锁、表级意向锁，相关的数据结构在项目结构文档中进行了介绍。

### 任务：加锁解锁操作

在完成本任务之前，可以先画出锁相容矩阵，再进行加锁解锁流程的梳理；同时，在当前任务中，锁的申请和释放不需要考虑死锁问题。

你需要完成`src/transaction/concurrency/lock_manager.cpp`文件中的以下接口：

#### （1）行级锁加锁

- `LockSharedOnRecord(Transaction *, const Rid, int)`：用于申请指定元组上的读锁。
  
  事务要对表中的某个指定元组申请行级读锁，该操作需要被阻塞直到申请成功或失败，如果申请成功则返回true，否则返回false。

- `LockExclusiveOnRecord(Transaction *, const Rid, int)`：用于申请指定元组上的写锁。
  
  事务要对表中的某个指定元组申请行级写锁，该操作需要被阻塞直到申请成功或失败，如果申请成功则返回true，否则返回false。

#### （2）表级锁加锁

- `LockSharedOnTable(Transaction *txn, int tab_fd)`：用于申请指定表上的读锁。
  
  事务要对表中的某个指定元组申请表级读锁，该操作需要被阻塞直到申请成功或失败，如果申请成功则返回true，否则返回false。

- `LockExclusiveOnTable(Transaction *txn, int tab_fd)`：用于申请指定表上的写锁。
  
  事务要对表中的某个指定元组申请表级写锁，该操作需要被阻塞直到申请成功或失败，如果申请成功则返回true，否则返回false。

- `LockISOnTable(Transaction *txn, int tab_fd)`：用于申请指定表上的意向读锁。
  
  事务要对表中的某个指定元组申请表级意向读锁，该操作需要被阻塞直到申请成功或失败，如果申请成功则返回true，否则返回false。

- `LockIXOnTable(Transaction *txn, int tab_fd)`：用于申请指定表上的意向写锁。
  
  事务要对表中的某个指定元组申请表级意向写锁，该操作需要被阻塞直到申请成功或失败，如果申请成功则返回true，否则返回false。

#### （3）解锁

- `Unlock(Transaction *, LockDataId)`：解锁操作。
- 需要更新锁表，如果解锁成功则返回true，否则返回false。

### 测试点及分数

```bash
cd build
make lock_manager_test  # 40分
./bin/lock_manager_test
```

## 实验三 并发控制实验（30分）

在本实验中，你需要在执行层和存储层的相关接口中，添加加锁和解锁操作，并且在适当的地方进行异常处理。
本实验目前要求支持可串行化隔离级别。

你需要修改`src/record/rm_file_handle.cpp`中的以下接口：

- `get_record(const Rid, Context *)`：在该接口中，你需要申请对应元组上的行级锁。
- `delete_record(const Rid, Context *)`：在该接口中，你需要申请对应元组上的行级锁。
- `update_record(const Rid, Context *)`：在该接口中，你需要申请对应元组上的行级锁。
- `get_record(const Rid, Context *)`：在该接口中，你需要申请对应元组上的行级锁。

同时还需要修改`src/system/sm_manager.cpp`和`executor_manager.cpp`中的相关接口，在合适的地方申请行级锁和意向锁。主要包括以下接口：

- `create_table(const std::string, const std::vector<ColDef>, Context *)`
- `drop_table(const std::string, Context *)`
- `create_index(const std::string, const std::string, Context *)`
- `drop_index(const std::string, const std::string, Context *)`
- `insert_into(const std::string, std::vector<Value>, Context *)`
- `delete_from(const std::string, std::vector<Condition>, Context *)`
- `update_set(const std::string, std::vector<SetClause>, std::vector<Condition>, Context *)`

**提示**：对于加锁和解锁过程中抛出的异常，你需要在合适的地方进行处理。

### 测试点及分数

```bash
cd build
make concurrency_test  # 30分
./bin/concurrency_test
```

