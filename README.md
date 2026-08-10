# RUCBase

```text
 ____  _   _  ____ ____    _    ____  _____
|  _ \| | | |/ ___| __ )  / \  / ___|| ____|
| |_) | | | | |   |  _ \ / _ \ \___ \|  _|
|  _ <| |_| | |___| |_) / ___ \ ___) | |___
|_| \_ \___/ \____|____/_/   \_\____/|_____|
```

RUCBase 是由中国人民大学卢卫教授带领数据库教学团队开发的一款教学用数据库管理系统，配套教育部“101计划”计算机核心教材《数据库管理系统原理与实现》建设，主要面向数据库基础较为薄弱的本科生，为数据库系统课程的实验教学提供支撑。

RUCBase 的系统框架参考并借鉴了 CMU 15-445 课程的 [BusTub](https://github.com/cmu-db/bustub) 和 Stanford CS346 课程的 [RedBase](https://web.stanford.edu/class/cs346/2015/redbase.html)。目前，中国人民大学、哈尔滨工业大学、华中科技大学、西北工业大学、西安电子科技大学等高校已使用 RucBase 开展数据库内核相关实验教学。

## 从这里开始

| 目的 | 文档 |
| --- | --- |
| 第一次使用 | [文档导航](docs/README.md) |
| 配置、编译、运行和测试 | [RUCBase 使用文档](docs/RUCBase使用文档.md) |
| 了解模块关系 | [RUCBase 项目结构](docs/RUCBase项目结构.md) |
| 修改框架代码 | [RUCBase 开发文档](docs/RUCBase开发文档.md) |
| 了解客户端协议 | [RUCBase 通信协议](docs/RUCBase通信协议.md) |

## 快速开始

推荐使用仓库提供的课程 Docker 环境。手工配置时建议使用 Ubuntu 24.04 LTS、GCC 11+ 或 Clang 14+、CMake 3.22+。

```bash
git clone --recursive https://github.com/ruc-deke/rucbase-lab.git
cd rucbase-lab
cmake --preset debug
cmake --build --preset debug-client -j 4
```

启动服务端：

```bash
cd build/debug
./bin/rmdb -p 8765 demo
```

在另一个终端启动客户端：

```bash
cd build/debug
./bin/rucbase_client -p 8765
```

完整环境说明、测试命令和常见问题见 [RUCBase 使用文档](docs/RUCBase使用文档.md)。

## 课程实验

请按课程进度完成对应实验，不要提前修改后续实验模块。

| 实验 | 主题 | 文档 |
| --- | --- | --- |
| Lab 1 | 磁盘、缓冲池和记录管理 | [存储管理](<docs/RUCBase-Lab1[存储管理].md>) |
| Lab 2 | B+ 树索引 | [索引管理](<docs/RUCBase-Lab2[索引管理].md>) |
| Lab 3 | 元数据、DDL 和查询执行 | [查询执行](<docs/RUCBase-Lab3[查询执行].md>) |
| Lab 4 | 事务、锁和并发控制 | [并发控制](<docs/RUCBase-Lab4[并发控制].md>) |

## 主要目录

```text
src/                  数据库内核、服务端和测试
rucbase_client/       课程客户端
docs/                 使用、开发、项目结构和实验文档
docker/               课程开发镜像
build/<preset>/       本地构建产物，不提交到 Git
```

## 学术诚信

RUCBase 实验用于课程学习。学生应独立完成实验核心代码，遵守课程关于代码共享、作业提交和公开仓库的规定，不要公开包含实验答案的个人仓库。

欢迎通过 Issue 或 Pull Request 报告文档错误、构建问题和可复现的框架缺陷。

## 许可

RUCBase 项目源码按 [MulanPSL-2.0](LICENSE) 发布。GoogleTest 等第三方组件及生成代码仍受其各自许可证约束。
