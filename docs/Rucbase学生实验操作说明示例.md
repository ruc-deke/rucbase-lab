<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->

- [实验操作指南](#%E5%AE%9E%E9%AA%8C%E6%93%8D%E4%BD%9C%E6%8C%87%E5%8D%97)
  - [补全代码](#%E8%A1%A5%E5%85%A8%E4%BB%A3%E7%A0%81)
  - [自我测试](#%E8%87%AA%E6%88%91%E6%B5%8B%E8%AF%95)
    - [扩展](#%E6%89%A9%E5%B1%95)
  - [想运行数据库？](#%E6%83%B3%E8%BF%90%E8%A1%8C%E6%95%B0%E6%8D%AE%E5%BA%93)
  - [如何提交](#%E5%A6%82%E4%BD%95%E6%8F%90%E4%BA%A4)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# 实验操作指南

> 本文档将以实验一（存储管理实验）部分任务为例示范你应该如何完成一个实验任务，并进行自我测试和提交

根据[Rucbase-Lab1存储管理实验文档](Rucbase-Lab1[存储管理实验文档].md)查看相应任务，进入`rucbase`项目对应的代码目录，你将看到实验文档中说明的需要你本人完成的项目源码`.cpp/.h`文件，除此之外，你还可能看到类似`*_test.cpp/*_test.sh`的文件，其中`*_test.cpp`文件是`GoogleTest`测试源码文件，而`.sh`文件则是在其他实验中使用的非`GoogleTest`测试脚本，你会在对应实验发布时获得相应的说明文件。

在阅读本指南前，请确保自己已经按照[Rucbase环境配置文档](Rucbase环境配置文档.md)配置好了环境，并根据[Rucbase使用文档](Rucbase使用文档.md)创建好了`build`目录，测试了`cmake`工具

## 补全代码

我们以`任务1.2 缓冲池替换策略`为例，进入`replacer`目录，其中源码文件如下：

```bash
Mode                 LastWriteTime         Length Name
----                 -------------         ------ ----
-a----         2022/9/18     10:07           1352 clock_replacer.cpp
-a----         2022/9/18     10:07           1252 clock_replacer.h
-a----         2022/9/18     10:07           2039 clock_replacer_test.cpp
-a----         2022/9/18     10:07            434 CMakeLists.txt
-a----         2022/9/18     10:07           1590 lru_replacer.cpp
-a----         2022/9/18     10:07           2351 lru_replacer.h
-a----         2022/9/18     10:07           9615 lru_replacer_test.cpp
-a----         2022/9/18     10:07           1021 replacer.h
```

为完成本任务，你需要完成[Rucbase-Lab1存储管理实验文档](Rucbase-Lab1[存储管理实验文档].md)中指出的`Replacer`类相应接口，其中必做任务是LRU策略，你需要打开`lru_replacer.cpp`，在源码文件中，对于每个你需要实现的接口方法进行了功能描述和实现提示，示例如下：

```cpp
/**
 * @brief 使用LRU策略删除一个victim frame，这个函数能得到frame_id
 * @param[out] frame_id id of frame that was removed, nullptr if no victim was found
 * @return true if a victim frame was found, false otherwise
 */
bool LRUReplacer::Victim(frame_id_t *frame_id) {
    // C++17 std::scoped_lock
    // 它能够避免死锁发生，其构造函数能够自动进行上锁操作，析构函数会对互斥量进行解锁操作，保证线程安全。
    std::scoped_lock lock{latch_};

    // Todo:
    //  利用lru_replacer中的LRUlist_,LRUHash_实现LRU策略
    //  选择合适的frame指定为淘汰页面,赋值给*frame_id

    return true;
}
```

请注意，返回值代码语句只是为了保证初始代码不会有语法检查错误，事实上整个接口内部的实现逻辑你可以完全修改。

如果你需要额外的数据结构，你可以在`.h`文件中对类进行添加，在`LRUReplacer`中，进入`lru_replacer.h`，你可以看到已经给出的部分数据结构。

```cpp
std::mutex latch_;               // 互斥锁
std::list<frame_id_t> LRUlist_;  // 按加入的时间顺序存放unpinned pages的frame id，首部表示最近被访问
std::unordered_map<frame_id_t, std::list<frame_id_t>::iterator> LRUhash_;  // frame_id_t -> unpinned pages的frame id
size_t max_size_;  // 最大容量（与缓冲池的容量相同）
```

你也可以自行修改或添加，只要最后正确实现功能即可。**但是，强烈建议你不要擅自修改每个接口的声明**

当你完成LRU策略后，你也可以自行考虑是否实现CLOCK策略，整个实现步骤与LRU任务类似。



## 自我测试

当你完成实验任务后，你可以对自己的实验进行功能测试。请首先回到整个项目的根目录

```bash
cd rucbase-lab
cd build
make [target_test]
./bin/[target_test]
```

如你想测试`lru_replacer_test.cpp`提供的测试，你的编译执行命令就是

```bash
make lru_replacer_test
./bin/lru_replacer_test
```

如果测试通过，你得到的输出应该类似于

```
aaron@DESKTOP-U1TJ26P:~/rucdeke/rucbase-lab/build$ ./bin/lru_replacer_test 
Running main() from /home/aaron/rucdeke/rucbase-lab/deps/googletest/googletest/src/gtest_main.cc
[==========] Running 6 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 6 tests from LRUReplacerTest
[ RUN      ] LRUReplacerTest.SampleTest
[       OK ] LRUReplacerTest.SampleTest (0 ms)
[ RUN      ] LRUReplacerTest.Victim
[       OK ] LRUReplacerTest.Victim (1 ms)
[ RUN      ] LRUReplacerTest.Pin
[       OK ] LRUReplacerTest.Pin (1 ms)
[ RUN      ] LRUReplacerTest.Size
[       OK ] LRUReplacerTest.Size (8 ms)
[ RUN      ] LRUReplacerTest.ConcurrencyTest
[       OK ] LRUReplacerTest.ConcurrencyTest (122 ms)
[ RUN      ] LRUReplacerTest.IntegratedTest
[       OK ] LRUReplacerTest.IntegratedTest (12 ms)
[----------] 6 tests from LRUReplacerTest (146 ms total)

[----------] Global test environment tear-down
[==========] 6 tests from 1 test suite ran. (146 ms total)
[  PASSED  ] 6 tests.
```

测试输出会给出各个测试点是否通过的信息以及每个测试点花费的时间。

如果没有通过，会在对应测试点打印不匹配的信息。你可以进行比对分析为什么自己的逻辑没能给出正确的输出。

### 扩展

整个项目是是由`cmake`管理的，如果你想知道编译命令为什么这么写，请打开每个模块下的`CMakeLists.txt`

```cmake
set(SOURCES lru_replacer.cpp clock_replacer.cpp)
add_library(lru_replacer STATIC ${SOURCES})
add_library(clock_replacer STATIC ${SOURCES})

add_executable(lru_replacer_test lru_replacer_test.cpp)
target_link_libraries(lru_replacer_test lru_replacer gtest_main)  # add gtest



add_executable(clock_replacer_test clock_replacer_test.cpp)
target_link_libraries(clock_replacer_test clock_replacer gtest_main)  # add gtest
```

这里将`lru_replacer_test.cpp`生成可执行文件`lru_replacer_test`，因此你需要`make lru_replacer_test`来生成测试可执行文件，其他实验测试命名以依据此规范。

## 想运行数据库？

当然可以！同样是以上文的`CMakeLists.txt`为例，你可以编译自己的库文件，如你自己实现了`lru_replacer`，你可以进行如下编译操作

```bash
make lru_replacer
-- Configuring done
-- Generating done
-- Build files have been written to: /home/aaron/rucdeke/rucbase-lab/build
Scanning dependencies of target lru_replacer
[ 33%] Building CXX object src/replacer/CMakeFiles/lru_replacer.dir/lru_replacer.cpp.o
[ 66%] Building CXX object src/replacer/CMakeFiles/lru_replacer.dir/clock_replacer.cpp.o
[100%] Linking CXX static library ../../lib/liblru_replacer.a
[100%] Built target lru_replacer
```

在`rucbase-lab/build/lib`下你可以看到你生成的`liblru_replacer.a`静态库文件，你可以将它替换掉我们提供的静态库文件，这样就可以编译一个以你自己实现的`lru_replacer`作为相应功能模块的数据库可执行文件。

## 如何提交

我们鼓励你使用`Github`或者`Gitee`的私有仓库功能，建立自己的远程代码库进行代码与报告提交，并将助教账户添加为你们仓库的协作者或者赋予访问权限。助教会定期拉取你们的仓库代码进行测试，并将测试结果贴在仓库的`issues`中。

你们也可以使用压缩包的方式将自己对应实验更改的代码文件以及相关报告发送到助教邮箱或者其他系统中，请根据个人喜好选择提交方式。

