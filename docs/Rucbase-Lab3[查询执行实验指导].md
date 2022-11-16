<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->

- [查询执行实验指导](#%E6%9F%A5%E8%AF%A2%E6%89%A7%E8%A1%8C%E5%AE%9E%E9%AA%8C%E6%8C%87%E5%AF%BC)
  - [任务一指导：DDL](#%E4%BB%BB%E5%8A%A1%E4%B8%80%E6%8C%87%E5%AF%BCddl)
    - [create_db操作思路](#create_db%E6%93%8D%E4%BD%9C%E6%80%9D%E8%B7%AF)
    - [close_db操作思路](#close_db%E6%93%8D%E4%BD%9C%E6%80%9D%E8%B7%AF)
    - [drop_table操作思路](#drop_table%E6%93%8D%E4%BD%9C%E6%80%9D%E8%B7%AF)
  - [任务三指导：算子接口实现](#%E4%BB%BB%E5%8A%A1%E4%B8%89%E6%8C%87%E5%AF%BC%E7%AE%97%E5%AD%90%E6%8E%A5%E5%8F%A3%E5%AE%9E%E7%8E%B0)
    - [Insert操作思路](#insert%E6%93%8D%E4%BD%9C%E6%80%9D%E8%B7%AF)
    - [Delete 操作思路](#delete-%E6%93%8D%E4%BD%9C%E6%80%9D%E8%B7%AF)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# 查询执行实验指导


## 任务一指导：DDL

实验对应代码文件`sm_manager.cpp`

在本实验中，你需要利用*nix相关文件操作创建和删除数据库实体文件，并利用记录和索引模块的相关接口完成数据库系统的操作的功能逻辑

### create_db操作思路

创建数据库的接口声明是这样的
```cpp
void SmManager::create_db(const std::string &db_name) ;
```
参数`db_name`指出了要创建的数据库名称，在本系统中，数据库在*nix系统中表现为一个文件夹，因此你需要首先判断该文件夹目录是否存在，并使用`mkdir`创建并进入该文件夹中

```cpp
	if (is_dir(db_name)) {
        throw DatabaseExistsError(db_name);
    }
    // Create a subdirectory for the database
    std::string cmd = "mkdir " + db_name;
    if (system(cmd.c_str()) < 0) {  // 创建一个名为db_name的目录
        throw UnixError();
    }
    if (chdir(db_name.c_str()) < 0) {  // 进入名为db_name的目录
        throw UnixError();
    }
```

创建目录后，你需要为该数据库构建`DBMeta`文件，这样`rucbase`才能识别并读取相关文件。利用已经写好的重载运算符创建。

```cpp
	// Create the system catalogs
    DbMeta *new_db = new DbMeta();
    new_db->name_ = db_name;

    // 注意，此处ofstream会在当前目录创建(如果没有此文件先创建)和打开一个名为DB_META_NAME的文件
    std::ofstream ofs(DB_META_NAME);

    // 将new_db中的信息，按照定义好的operator<<操作符，写入到ofs打开的DB_META_NAME文件中
    ofs << *new_db;  // 注意：此处重载了操作符<<

```

最后，释放内存中的`DBMeta`,并回退到上一级目录

```cpp
    delete new_db;

    // cd back to root dir
    if (chdir("..") < 0) {
        throw UnixError();
    }
```

### close_db操作思路

关闭数据库的接口声明是这样的

```cpp
void SmManager::close_db() 
```

此时已经获取了一个打开的数据库，需要将其关闭。你需要阅读`open_db()`和`class SmManager `，利用获取的`DBMeta`成员变量将`meta`数据`Dump`到文件中

```cpp
std::ofstream ofs(DB_META_NAME);
// 参考create_db
```

接着你需要清理`SmManager.db_`，让系统知道现在的`db_`已经重置了

```cpp
db_.name_.clear();
db_.tabs_.clear();
```

关闭数据库表的记录文件和索引文件

```cpp
    // Close all record files
    for (auto &entry : fhs_) {
        rm_manager_->close_file(entry.second.get());
    }
	fhs_.clear();
	// Close all index files
	// ...

```

最后，回退到上级目录

```cpp
if (chdir("..") < 0) {
        throw UnixError();
}
```

### drop_table操作思路

删表操作的声明是这样的

```cpp
void SmManager::drop_table(const std::string &tab_name, Context *context) 
```

context在日志实验中完成，目前你可以不关注这个参数

删除表时，你需要关闭并删除记录文件，并且检查表上是否有索引，把索引文件也要关闭和删除

为获取表索引信息，首先，你需要获取表元数据`TabMeta`

```cpp
TabMeta &tab = db_.get_table(tab_name);
```

接着，你需要删除记录文件

```cpp
rm_manager_->close_file(...);
rm_manager_->destroy_file(...);
```

删除索引文件

```cpp
for (auto &col : tab.cols) {
     if (col.index) {
     	// drop_index
     }
}
```

最后在数据库元数据文件和记录文件句柄中删除该表信息

```cpp
db_.tabs_.erase(...); 
fhs_.erase(...)
```



## 任务三指导：算子接口实现 

实验对应代码文件: `executor_*`

在本实验中，你需要实现每个算子的Next接口。这里给出部分示例或思路，你可以据此简单了解`rucbase`如何进行`数据操作的

###  Insert操作思路

insert语句的语法结构是这样的：

```sql
insert into table_name values (?,?,?,?);
```

我们先查看Insert算子的构造方法参数：

```cpp
InsertExecutor(SmManager *sm_manager, const std::string &tab_name, std::vector<Value> values, Context *context)
```

SmManager是负责管理数据库中db, table, index的元数据，支持create/drop/open/close等操作的类，每个SmManager对应一个db文件。你可以查看它的相关成员:

```cpp
public:
    DbMeta db_;  // create_db时将会将DbMeta写入文件，open_db时将会从文件中读出DbMeta
    std::unordered_map<std::string, std::unique_ptr<RmFileHandle>> fhs_;   // file name -> record file handle
    std::unordered_map<std::string, std::unique_ptr<IxIndexHandle>> ihs_;  // file name -> index file handle
   private:
    DiskManager *disk_manager_;
    BufferPoolManager *buffer_pool_manager_;
    RmManager *rm_manager_;
    IxManager *ix_manager_;
```

其中,`fhs_`和`ihs_`是记录文件和索引文件的句柄哈希表，可以通过其对记录或者索引进行操作，这也是算子中需要的关键变量。

在算子构造方法中，tab_name明显是目标操作的表，对应语法结构的`table_name`，而values根据算子插入类型，对应语法结构的`(?,?,?,?)`

查看`RmFileHandle`的方法

```cpp
Rid insert_record(char *buf, Context *context);  //*

void insert_record(const Rid &rid, char *buf);

void delete_record(const Rid &rid, Context *context);

void update_record(const Rid &rid, char *buf, Context *context);
```

由于现在我们是要新建一个record，所以使用返回值Rid的insert_record方法从`buf`中构建一个record插入并返回Rid

`buf`是我们根据`values`构建的record缓存，因此我们通过该表对应的record大小这样构建一个RmRecord结构体作为`buf`：

```cpp
// Make record buffer
RmRecord rec(fh_->get_file_hdr().record_size);
```

构建的record rec虽然大小和目标表一致，但是目前还是没有值的，所以我们把`values`填充进去：

```cpp
for (size_t i = 0; i < values_.size(); i++) {
            auto &col = tab_.cols[i];
            auto &val = values_[i];
            if (col.type != val.type) {
                throw IncompatibleTypeError(coltype2str(col.type), coltype2str(val.type));
            }
            val.init_raw(col.len);
            memcpy(rec.data + col.offset, val.raw->data, col.len);
}
```

现在我们就可以调用函数*来插入数据了

```cpp
 // Insert into record file
rid_ = fh_->insert_record(rec.data, context_);
```

注意，目标表可能是有索引的！插入数据后还应该更新索引。索引也是文件，我们也可以用句柄操作，此时的句柄就变成了`_ihs`，利用SmManager，我们获取目标表的列的索引，对其进行插入entry操作

```cpp
// Insert into index
for (size_t i = 0; i < tab_.cols.size(); i++) {
       auto &col = tab_.cols[i];
       //...
}
```

得到了`ColMeta`，查看一下`ColMeta`的结构，其中有一个布尔值`index`，标记了该列是否存在索引，以此我们来决定是否进行索引更新操作。

为进行索引操作，回顾索引层`IxIndexHandle`，查看其中的方法

```cpp
bool insert_entry(const char *key, const Rid &value, Transaction *transaction);

bool delete_entry(const char *key, Transaction *transaction);
```

`key`值即本record对应的列值，通过记录偏移获取，因此索引插入实现如下：

```cpp
if (col.index) {
	auto ih = sm_manager_->ihs_.at(sm_manager_->get_ix_manager()->get_index_name(tab_name_, i)).get();
	ih->insert_entry(rec.data + col.offset, rid_);
}
```

### Delete 操作思路

回顾Insert的思路，与之类似，构造方法中的`std::vector<Rid> rids`是根据扫描算子得到的应该删除的记录组，你可以先获取`IxIndexHandle`删除对应的索引`entry`，再获取`RmFileHandle`删除记录。







