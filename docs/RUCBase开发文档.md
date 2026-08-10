# RUCBase 开发文档

本文面向维护者和希望改进框架代码的贡献者，说明源码修改约定。环境、编译和测试命令统一放在 [RUCBase 使用文档](RUCBase使用文档.md)；实验任务统一放在四份 Lab 文档中。

## 1. 修改原则

- 尽量让一次修改只解决一个问题，避免顺手重构无关模块。
- 不修改课程实验公开接口，除非课程框架同步调整了文档和测试。
- 不提交 `build/`、IDE 配置、临时数据库、日志或 Flex/Bison 生成文件。
- 面向学生的文字、注释和界面统一使用名称 `RUCBase`；可执行文件 `rmdb` 保持不变。

## 2. 构建目标

项目主要由三个库和两个可执行文件组成：

| 目标 | 内容 |
| --- | --- |
| `rucbase_core` | 存储、索引、执行、事务等数据库内核模块 |
| `rucbase_parser` | Flex/Bison 解析器 |
| `rucbase_wire` | 服务端和客户端共用的通信协议 |
| `rmdb` | 数据库服务端 |
| `rucbase_client` | 课程客户端 |

模块之间的关系见 [RUCBase 项目结构](RUCBase项目结构.md)。

## 3. Parser 修改

`src/parser/lex.l` 和 `src/parser/yacc.y` 是词法、语法的源文件。CMake 会在构建目录中生成并编译 `lex.yy.cpp`、`yacc.tab.cpp` 和相关头文件，不要手工生成到 `src/parser/`，也不要提交生成文件。

Parser 文件职责：

- `ast.h`：SQL 抽象语法树；
- `lex.l`：把 SQL 文本转换为 token；
- `yacc.y`：把 token 组合成 AST；
- `parser.h/.cpp`：对外提供 `Parse(sql)` 和错误诊断；
- `parser_internal.h`：Flex/Bison 共用的内部数据，其他模块不应包含。

修改 Parser 后重新配置并构建即可，重点运行 `test_parser` 和 `analyze_planner_test`。

## 4. 代码格式

项目使用 C++20，格式以仓库根目录的 `.clang-format` 为准。不要在文档中复制另一份格式配置。

格式化本次修改的 C++ 文件：

```bash
clang-format -i path/to/file.cpp path/to/file.h
```

命名和排版尽量与所在模块保持一致。不要为了格式化而改动无关文件。

## 5. 注释和文档

- 公共类型和接口使用简洁的 Doxygen 注释。
- 非平凡的校验、所有权、状态恢复、持久化和回滚逻辑应说明契约或不变量。
- 简单赋值、访问器和能直接从代码看出的操作不需要逐行解释。
- 根据需要使用 `@brief`、`@param`、`@return`、`@throws`、`@pre` 和 `@post`，标签后不加冒号。
- 修改命令、路径或公开接口时，同步检查 README、使用文档、项目结构和对应 Lab 文档。

示例：

```cpp
/**
 * @brief 打开数据库并加载元数据与文件句柄。
 * @param db_name 数据库目录名。
 * @throws DatabaseNotFoundError 数据库目录或元数据不存在。
 * @post 当前工作目录切换到已打开的数据库目录。
 */
void open_db(const std::string& db_name);
```

## 6. 测试和提交

修改后至少运行与改动直接相关的测试。涉及公共构建、Parser、通信协议或服务端入口时，再运行相应的跨平台 smoke tests。不要通过放宽断言、跳过测试或修改标准答案来掩盖失败。

提交前检查：

```bash
git status --short
git diff --check
```

Pull Request 应说明修改目的、验证命令和仍然存在的限制。课程实验答案不得提交到公开仓库。
