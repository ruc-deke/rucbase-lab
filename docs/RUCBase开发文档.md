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

Parser 文件职责（详细扩展流程见 [`src/parser/README.md`](../src/parser/README.md)）：

- `ast.h`：完整语句、公共子句及递归 FROM/JOIN 树；
- `expression.h`：字面量、列、谓词、函数和子查询表达式；
- `lex.l`：把 SQL 文本转换为 token；
- `yacc.y`：把 token 组合成 AST；
- `parser.h/.cpp`：对外提供 `Parse(sql)` 和错误诊断；
- `parser_internal.h`：Flex/Bison 共用的内部数据，其他模块不应包含。

修改 Parser 后重新配置并构建即可，重点运行 `framework_parser_test` 和 `framework_query_planning_test`。

## 4. 代码格式

项目使用 C++20，格式以仓库根目录的 `.clang-format` 为准。不要在文档中复制另一份格式配置。

```bash
cmake --preset quality
clang-format -i path/to/changed_file.cpp
```

日常开发只格式化自己修改的 C++ 文件。`format-cpp` 会格式化整个源码树，只能在专门的机械清理提交中使用，
不要把全仓格式化与功能修改混在一起。`deps/`、构建目录和 Flex/Bison 生成文件不参与格式检查。

## 5. 静态分析与严格构建

根目录 `.clang-tidy` 开启 `clang-analyzer`、`bugprone`、`performance` 和 `portability` 中经过筛选的规则。
`src/test/.clang-tidy` 继承根配置，并关闭只会干扰测试程序的规则。质量环境固定使用 LLVM 18；其他 LLVM 版本可以用于
本地预检查，但静态分析结果以 CI 为准。

日常检查：

```bash
cmake --build --preset quality --parallel 4
cmake --build --preset quality --target check-clang-tidy --parallel 4
ctest --preset quality-smoke
```

`quality` preset 启用 `-Wextra -Wpedantic -Werror`（MSVC 对应 `/W4 /WX`）。教学骨架允许尚未使用的参数和成员；
白盒测试允许用于暴露内部状态的关键字宏，其他编译告警仍会导致严格构建失败。

检查全仓 C++ 格式时使用：

```bash
cmake --build --preset quality --target check-format-cpp
```

不要通过扩大 `-Wno-*`、`NOLINT` 或 clang-tidy 排除列表来掩盖问题。确有误报时，例外应尽量
限定到具体测试、源文件或检查项，并在配置旁说明原因。

## 6. 注释和文档

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

## 7. 测试和提交

修改后至少运行与改动直接相关的测试。涉及公共构建、Parser、通信协议或服务端入口时，再运行相应的跨平台 smoke tests。不要通过放宽断言、跳过测试或修改标准答案来掩盖失败。

提交前检查：

```bash
git status --short
git diff --check
cmake --build --preset quality --target check-clang-tidy --parallel 4
```

Pull Request 应说明修改目的、验证命令和仍然存在的限制。课程实验答案不得提交到公开仓库。

CI 的 `Quality gates (Clang)` 在每个 Pull Request 上执行严格编译、clang-tidy 和 smoke tests；
`ASan + UBSan smoke tests` 同步检查内存与未定义行为。每周夜间任务额外执行 TSan smoke tests。
仓库维护者应把这两个 Pull Request job 设置为分支保护的必需检查。

本地复现 Sanitizer smoke tests：

```bash
cmake --preset sanitizers
cmake --build --preset sanitizers --parallel 4
ctest --preset sanitizers-smoke

cmake --preset tsan
cmake --build --preset tsan --parallel 4
ctest --preset tsan-smoke
```
