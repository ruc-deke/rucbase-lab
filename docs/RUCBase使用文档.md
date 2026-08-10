# RUCBase 使用文档

本文只介绍环境准备、编译、运行和测试。各实验需要完成的任务请查看对应的 Lab 文档；代码模块之间的关系请查看 [RUCBase 项目结构](RUCBase项目结构.md)。

## 1. 准备环境

### 1.1 使用课程 Docker 环境（推荐）

Windows 和 macOS 用户推荐安装 Docker Desktop、VS Code 和 Dev Containers 扩展，然后：

1. 递归克隆本仓库；
2. 使用 VS Code 打开仓库；
3. 执行 `Dev Containers: Reopen in Container`。

仓库中的 `.devcontainer/devcontainer.json` 会使用课程镜像，并把本地源码挂载到容器内。课程镜像同时支持 x64 和 ARM64。

### 1.2 手工配置 Ubuntu

推荐使用 Ubuntu 24.04 LTS。Windows 用户也可以使用 WSL 2 中的 Ubuntu。

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake flex bison python3 python3-pytest
```

最低要求：

| 工具 | 版本 |
| --- | --- |
| GCC | 11 及以上，推荐 13 |
| Clang | 14 及以上 |
| CMake | 3.22 及以上 |
| C++ 标准 | C++20 |

macOS 可以使用 Homebrew 安装 `cmake`、`flex` 和 `bison`，再通过 Python 环境安装 pytest。课程评分环境以 Ubuntu 为准。

## 2. 下载项目

```bash
git clone --recursive https://github.com/ruc-deke/rucbase-lab.git
cd rucbase-lab
```

如果克隆时没有下载子模块，可以补充执行：

```bash
git submodule update --init --recursive
```

GoogleTest 的查找顺序是：仓库子模块、系统安装包、CMake 自动下载。离线环境应提前准备子模块；不希望 CMake 访问网络时，可以增加 `-DRUCBASE_FETCH_DEPENDENCIES=OFF`。

## 3. 编译

在仓库根目录执行：

```bash
cmake --preset debug
cmake --build --preset debug-client -j 4
```

`debug-client` 只构建服务端 `rmdb` 和课程客户端 `rucbase_client`。需要编译测试时执行：

```bash
cmake --build --preset debug -j 4
```

构建产物位于 `build/debug/bin/`。常用预设如下：

| Preset | 用途 |
| --- | --- |
| `debug` | 日常开发和调试 |
| `release` | 优化构建 |
| `asan` | 检查内存错误 |
| `ubsan` | 检查未定义行为 |
| `tsan` | 检查数据竞争 |

只编译一个目标时，可以使用：

```bash
cmake --build --preset debug --target lru_replacer_test -j 4
```

修改 `CMakeLists.txt`、切换分支或新增源码文件后，重新执行 `cmake --preset debug` 即可，一般不需要删除整个构建目录。

## 4. 运行

先在一个终端启动服务端：

```bash
cd build/debug
./bin/rmdb -p 8765 demo
```

`demo` 是数据库名。数据库不存在时会自动创建；服务端默认监听 `127.0.0.1`。

再在另一个终端启动客户端：

```bash
cd build/debug
./bin/rucbase_client -h 127.0.0.1 -p 8765
```

客户端还支持直接执行一条 SQL 或一个脚本文件：

```bash
./bin/rucbase_client -p 8765 -e "show database;"
./bin/rucbase_client -p 8765 -e "show tables;"
./bin/rucbase_client -p 8765 -f demo.sql
```

交互模式下用分号结束一条语句。输入 `exit;`、`bye;` 或按 Ctrl-D 退出客户端；在服务端终端按 Ctrl-C 正常关闭服务端。

## 5. 测试

运行所有已经注册的测试：

```bash
ctest --preset debug --output-on-failure
```

按类别运行：

```bash
# GoogleTest、解析器和通信协议测试
ctest --preset debug -L unit --output-on-failure

# 查询、事务和并发黑盒测试
ctest --preset debug -L blackbox --output-on-failure
```

运行单个测试：

```bash
ctest --preset debug -R 'lru_replacer_test' --output-on-failure
```

黑盒测试需要 pytest。测试程序会为每个用例创建独立的临时数据库并选择动态端口；失败日志位于 `build/debug/test-logs/`。

各 Lab 文档还保留了与课程讲义兼容的 Python 测试入口。测试范围和评分以对应 Lab 文档为准。

## 6. 完成一次实验的建议流程

1. 阅读对应 Lab 文档，确认任务范围和允许修改的接口；
2. 找到源码中的 `Todo` 和接口注释，先梳理不变量与边界情况；
3. 只构建当前任务对应的测试目标；
4. 先运行小范围测试，再运行该 Lab 的完整测试；
5. 根据失败信息和 `build/debug/test-logs/` 定位问题；
6. 提交前检查 `git diff`，确认没有提交构建产物、数据库目录或调试文件。

学生应独立完成实验核心代码，并按照课程要求使用私有仓库或指定平台提交，不要公开包含实验答案的仓库。

## 7. 常见问题

### CMake 找不到 GoogleTest

优先执行：

```bash
git submodule update --init --recursive
cmake --preset debug
```

### CTest 中没有黑盒测试

确认 Python 可以导入 pytest，然后重新配置：

```bash
python3 -m pytest --version
cmake --preset debug
```

### 端口已被占用

为服务端和客户端指定同一个其他端口，例如 `-p 9000`。不要把 `kill -9` 作为正常关闭服务端的方式。
