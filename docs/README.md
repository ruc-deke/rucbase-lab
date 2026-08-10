# RUCBase 文档

第一次使用 RUCBase，请先阅读 [RUCBase 使用文档](RUCBase使用文档.md)，再进入当前课程要求的 Lab。

## 快速导航

| 想做什么 | 阅读文档 |
| --- | --- |
| 配置环境、编译、运行或测试 | [RUCBase 使用文档](RUCBase使用文档.md) |
| 了解代码模块和一次 SQL 的执行过程 | [RUCBase 项目结构](RUCBase项目结构.md) |
| 了解代码格式、Parser 和贡献约定 | [RUCBase 开发文档](RUCBase开发文档.md) |
| 完成 Lab 1 | [Lab 1：存储管理](<RUCBase-Lab1[存储管理].md>) |
| 完成 Lab 2 | [Lab 2：索引管理](<RUCBase-Lab2[索引管理].md>) |
| 完成 Lab 3 | [Lab 3：查询执行](<RUCBase-Lab3[查询执行].md>) |
| 完成 Lab 4 | [Lab 4：并发控制](<RUCBase-Lab4[并发控制].md>) |
| 了解客户端与服务端的帧格式 | [RUCBase 通信协议](RUCBase通信协议.md) |

## 文档职责

- 使用文档只维护环境、编译、运行和通用测试方法。
- 项目结构只说明模块边界和调用关系，不复制具体类实现。
- 开发文档只维护代码修改和贡献约定。
- Lab 文档只维护该实验的任务、提示、测试点和评分规则。

如果不同文档中的命令不一致，以使用文档和仓库中的 `CMakePresets.json`、`CMakeLists.txt` 为准。
