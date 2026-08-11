<div align="center">

<img src="docs/pics/RUCBase.png" alt="RUCBase" width="680">

**RUCBase ：面向数据库系统课程的教学型关系数据库管理系统**

[![RUCBase CI](https://github.com/ruc-deke/rucbase-lab/actions/workflows/ci.yml/badge.svg?branch=main&event=push)](https://github.com/ruc-deke/rucbase-lab/actions/workflows/ci.yml)
[![License: MulanPSL-2.0](https://img.shields.io/badge/License-MulanPSL--2.0-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C.svg?logo=c%2B%2B&logoColor=white)](CMakeLists.txt)

[快速开始](#quick-start) · [文档导航](docs/README.md) · [课程实验](#labs) · [参与开发](docs/RUCBase开发文档.md)

</div>

## 项目简介

RUCBase 是由中国人民大学卢卫教授带领数据库教学团队开发的一款教学用数据库管理系统，配套教育部“101计划”计算机核心教材《数据库管理系统原理与实现》建设，主要面向数据库系统课程初学者，为本科数据库系统课程的实验教学提供支撑。

RUCBase 的系统框架参考并借鉴了 CMU 15-445 课程的 [BusTub](https://github.com/cmu-db/bustub) 和 Stanford CS346 课程的 [RedBase](https://web.stanford.edu/class/cs346/2015/redbase.html)。目前，中国人民大学、哈尔滨工业大学、华中科技大学、西北工业大学、西安电子科技大学等高校已使用 RUCBase 开展数据库内核相关实验教学。

## 从这里开始

| 目的                   | 文档                                        |
|------------------------|---------------------------------------------|
| 查看全部文档           | [文档导航](docs/README.md)                  |
| 配置、编译、运行和测试 | [RUCBase 使用文档](docs/RUCBase使用文档.md) |
| 了解系统模块和目录结构 | [RUCBase 项目结构](docs/RUCBase项目结构.md) |
| 修改或扩展框架代码     | [RUCBase 开发文档](docs/RUCBase开发文档.md) |
| 了解客户端通信协议     | [RUCBase 通信协议](docs/RUCBase通信协议.md) |

<h2 id="quick-start">快速开始</h2>

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

<h2 id="labs">课程实验</h2>

课程教学中，建议按照实验顺序逐步完成各模块，并避免提前修改后续实验涉及的框架代码。

| 实验  | 主题                   | 文档                                         |
|-------|------------------------|----------------------------------------------|
| Lab 1 | 磁盘、缓冲池和记录管理 | [存储管理](<docs/RUCBase-Lab1[存储管理].md>) |
| Lab 2 | B+ 树索引              | [索引管理](<docs/RUCBase-Lab2[索引管理].md>) |
| Lab 3 | 元数据、DDL 和查询执行 | [查询执行](<docs/RUCBase-Lab3[查询执行].md>) |
| Lab 4 | 事务、锁和并发控制     | [并发控制](<docs/RUCBase-Lab4[并发控制].md>) |

## 主要目录

```text
src/                  数据库内核、服务端和测试
rucbase_client/       课程客户端
docs/                 使用、开发、项目结构和实验文档
docker/               课程开发镜像
```


## 许可

RUCBase 项目源码按 [MulanPSL-2.0](LICENSE) 发布。GoogleTest 等第三方组件及生成代码仍受其各自许可证约束。
