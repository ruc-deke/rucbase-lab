# RucBase
```
 ____  _   _  ____ ____    _    ____  _____ 
|  _ \| | | |/ ___| __ )  / \  / ___|| ____|
| |_) | | | | |   |  _ \ / _ \ \___ \|  _|  
|  _ <| |_| | |___| |_) / ___ \ ___) | |___ 
|_| \_ \___/ \____|____/_/   \_\____/|_____|
```

RucBase是由中国人民大学卢卫教授领导的数据库教学团队开发，配套教育部“101计划”计算机核心教材《数据库管理系统原理与实现》建设的教学用数据库管理系统，旨在支撑面向本科数据库零基础学生的数据库系统课程实验教学。RucBase系统框架部分参考和借鉴了CMU15-445课程的[BusTub](https://github.com/cmu-db/bustub) 和Standford CS346课程的[Redbase](https://web.stanford.edu/class/cs346/2015/redbase.html)，目前中国人民大学、哈尔滨工业大学、华中科技大学、西北工业大学、西安电子科技大学等高校使用该教学系统进行数据库内核实验。

## 快速导航

| 目的 | 入口 |
| --- | --- |
| 直接使用开发环境 | [Docker 开发镜像](docker/README.md) |
| 手工配置环境 | [环境配置文档](docs/RucBase环境配置文档.md) |
| 编译、运行和测试 | [使用文档](docs/RucBase使用文档.md) |
| 了解代码组织 | [项目结构](docs/RucBase项目结构.pdf) |
| 了解客户端协议 | [RMDB Wire Protocol](docs/rmdb_wire.md) |
| 开始课程实验 | [学生实验操作说明](docs/RucBase学生实验操作说明示例.md) |

## 主要组成

- `rmdb`：数据库服务端，负责数据库、SQL 执行和客户端连接。
- `rucbase_client`：课程方提供的交互式 SQL 客户端，支持交互执行、单条 SQL 和脚本文件。
- `src/test`：GoogleTest 单元测试、C++ 黑盒客户端和 pytest 黑盒测试。
- `docs`：环境、使用、开发和分阶段实验文档。


## 快速开始

### 方式一：使用 Docker（推荐）

课程开发镜像基于 Ubuntu 24.04 LTS，包含 C++ 编译工具链、CMake、pytest、调试工具，
以及构建时获取的最新版 Codex CLI 和 Claude Code。账号凭据不会写入镜像。

镜像同时提供 `linux/amd64` 和 `linux/arm64`。请根据主机架构拉取：

**x64：Windows Docker Desktop（Linux 容器模式）或 Intel Mac**

```bash
docker pull --platform linux/amd64 \
  crpi-i42psj2r9mqzm5eq.cn-wulanchabu.personal.cr.aliyuncs.com/daojiagban2026/rucbase-dev:latest
```

**ARM64：Apple Silicon Mac（M1/M2/M3/M4/M5）**

```bash
docker pull --platform linux/arm64 \
  crpi-i42psj2r9mqzm5eq.cn-wulanchabu.personal.cr.aliyuncs.com/daojiagban2026/rucbase-dev:latest
```

首次创建容器：

```bash
# Windows / Intel Mac
docker run --name rucbase-dev --platform linux/amd64 -it \
  crpi-i42psj2r9mqzm5eq.cn-wulanchabu.personal.cr.aliyuncs.com/daojiagban2026/rucbase-dev:latest

# Apple Silicon Mac
docker run --name rucbase-dev --platform linux/arm64 -it \
  crpi-i42psj2r9mqzm5eq.cn-wulanchabu.personal.cr.aliyuncs.com/daojiagban2026/rucbase-dev:latest
```

以后重新进入容器：

```bash
docker start -ai rucbase-dev
```

进入容器后，在源码目录中构建：

```bash
cd /workspace/RucBase-lab
cmake --preset debug
cmake --build --preset debug-client -j 4
```

镜像构建细节和视频教程见 [docker/README.md](docker/README.md) 与
[环境配置文档](docs/RucBase环境配置文档.md)。

### 方式二：在 Ubuntu 或 macOS 上手工构建

基本要求：

| 工具 | 要求 |
| --- | --- |
| 操作系统 | Ubuntu 24.04 LTS；macOS 可用于本地开发 |
| C++ 编译器 | GCC 11+（推荐 GCC 13）或 Clang 14+ |
| CMake | 3.22+ |
| 其他依赖 | flex、bison、readline、Python 3 和 pytest |
| C++ 标准 | C++17 |

下载项目（建议递归获取 GoogleTest 子模块）：

```bash
git clone --recursive https://github.com/ruc-deke/RucBase-lab.git
cd RucBase-lab
```

如果已经普通克隆过项目，可补充子模块：

```bash
git submodule update --init --recursive
```

配置并构建 Debug 版本：

```bash
cmake --preset debug
cmake --build --preset debug-client -j 4
```

构建产物位于 `build/debug/bin/`。需要完整构建（包括测试目标）时执行：

```bash
cmake --build --preset debug -j 4
```

## 运行 RucBase

服务端和客户端使用 TCP 连接。建议在构建目录中运行，使数据库目录和日志位置清晰：

```bash
cd build/debug
./bin/rmdb -p 8765 demo
```

服务端启动后，在另一个终端连接：

```bash
cd build/debug
./bin/rucbase_client -h 127.0.0.1 -p 8765
```

客户端也支持单条 SQL 和脚本文件：

```bash
./bin/rucbase_client -h 127.0.0.1 -p 8765 -e "show tables;"
./bin/rucbase_client -h 127.0.0.1 -p 8765 -f demo.sql
```

交互模式下以分号结束一条语句；输入 `exit;`、`bye;` 或按 Ctrl-D 退出客户端。
服务端使用 Ctrl-C 关闭。更多 SQL 示例见 [使用文档](docs/RucBase使用文档.md)。

## 构建配置与测试

仓库通过 `CMakePresets.json` 提供统一配置：

| Preset | 用途 |
| --- | --- |
| `debug` | 日常开发和调试 |
| `release` | 优化构建 |
| `asan` | AddressSanitizer |
| `ubsan` | UndefinedBehaviorSanitizer |
| `tsan` | ThreadSanitizer |

运行全部已注册测试：

```bash
ctest --preset debug --output-on-failure
```

按类别运行：

```bash
# GoogleTest、解析器和 Wire 协议单元测试
ctest --preset debug -L unit --output-on-failure

# pytest 黑盒测试：每个测试使用独立临时数据库和动态端口
ctest --preset debug -L blackbox --output-on-failure
```

测试失败时，黑盒测试日志保存在 `build/debug/test-logs/`。如果本机未安装 pytest，
黑盒测试不会被 CMake 注册；Docker 镜像已包含 `python3-pytest`。

## 课程实验路线

| 实验 | 核心主题 | 建议投入 |
| --- | --- | ---: |
| 环境配置 | Docker、编译器、CMake、调试工具 | 1–2h |
| 存储管理 | 磁盘管理、缓冲池、记录管理 | 约 15h |
| 索引管理 | B+ 树和索引扫描 | 约 35h |
| 查询执行 | 解析、优化、执行器和 SQL 查询 | 30–40h |
| 并发控制 | 事务、锁和并发调度 | 25–30h |

实验要求和评分以各实验文档为准：

- [Lab 1：存储管理](<docs/RucBase-Lab1[存储管理实验文档].md>)
- [Lab 2：索引管理](<docs/RucBase-Lab2[索引管理实验文档].md>)
- [Lab 3：查询执行](<docs/RucBase-Lab3[查询执行实验文档].md>)
- [Lab 4：并发控制](<docs/RucBase-Lab4[并发控制实验文档].md>)

## 仓库结构

```text
.
├── src/                  # 数据库内核、网络协议和测试
│   ├── storage/          # 磁盘、缓冲池和记录管理
│   ├── index/            # B+ 树和索引
│   ├── parser/           # flex/bison 词法和语法分析
│   ├── analyze/          # 语义分析
│   ├── optimizer/        # 查询计划和优化
│   ├── execution/        # 执行器和查询执行
│   ├── transaction/      # 事务与并发控制
│   ├── recovery/         # 日志与恢复
│   └── net/              # RucBase Wire Protocol
├── rucbase_client/       # 官方交互客户端
├── src/test/             # 单元、黑盒和实验测试
├── docs/                 # 使用、开发、环境和实验文档
├── docker/               # 教学开发镜像
├── CMakeLists.txt
└── CMakePresets.json
```

## 学术诚信与贡献

RucBase 的实验代码用于课程学习。学生应独立完成实验，并遵守课程关于代码共享、作业提交
和公开仓库的规定；不要直接公开包含实验答案的个人仓库。

欢迎通过 Issue 或 Pull Request 报告文档错误、构建问题和可复现的缺陷。提交代码前请先
确认 `cmake --preset debug`、相关构建目标和测试能够通过。

## 许可

RucBase 项目源码按 [MIT License](LICENSE) 发布。Readline、GoogleTest 等第三方组件仍
受其各自许可证约束；分发包含这些组件的二进制或镜像时，请同时遵守相应许可证要求。
