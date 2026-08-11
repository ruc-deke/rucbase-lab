# 第一次修改 RUCBase Parser

这份文档写给第一次接触数据库 Parser、Flex、Bison 和 AST 的同学。

第一次阅读时，不需要把整份文档背下来。建议只做四件事：

1. 看懂下面的“一条 SQL 怎样变成 AST”；
2. 认识 `lex.l` 和 `yacc.y`；
3. 按“第一次新增语法”的步骤做一个小修改；
4. 运行 Parser 测试。

遇到报错时，再查后面的“常见问题”。

## 1. 一条 SQL 怎样变成 AST

先看这条 SQL：

```sql
SELECT id FROM student WHERE age >= 18;
```

它会经过三步。

### 第一步：Lexer 切词

Lexer 可以理解成“切词器”。它把 SQL 字符串切成 token：

```text
SELECT
IDENTIFIER("id")
FROM
IDENTIFIER("student")
WHERE
IDENTIFIER("age")
GEQ
VALUE_INT(18)
';'
```

其中：

- SELECT、FROM、WHERE 是关键字；
- id、student、age 都是标识符；
- `>=` 是运算符；
- 18 是整数字面量。

本项目使用 Flex 编写 Lexer，规则位于 `lex.l`。

### 第二步：Parser 按语法拼句子

Parser 检查这些 token 能否组成合法 SQL。例如它知道：

```text
SELECT 后面应该有投影列
FROM 后面应该有表
WHERE 后面应该有条件
```

本项目使用 Bison 编写 Grammar（语法规则），规则位于 `yacc.y`。

### 第三步：生成 AST

AST 是一棵 C++ 对象树。上面的 SQL 大致会变成：

```text
SelectStmt
├── select_items
│   └── Col("", "id")
├── from
│   └── TableRef("student")
└── where
    └── BinaryExpr
        ├── lhs: Col("", "age")
        ├── op:  Ge
        └── rhs: IntLit(18)
```

AST 只记录用户写了什么。它暂时不知道 student 表是否存在，也不知道 age 是不是整数。

后面的处理流程是：

```text
SQL
 ↓
Lexer：切成 token
 ↓
Parser：生成 AST
 ↓
Analyzer：检查表、列和类型
 ↓
Planner：生成查询计划
 ↓
Execution：执行计划
```

## 2. Parser 文件夹里有什么

第一次修改时，主要关注这几个文件：

| 文件 | 用大白话解释 |
| --- | --- |
| `lex.l` | 规定怎样识别 SELECT、表名、数字、`>=` 等 token |
| `yacc.y` | 规定 token 怎样组成 SQL，并创建 AST |
| `expression.h` | 定义列、数字、比较、AND/OR、函数等表达式节点 |
| `ast.h` | 定义 SELECT、INSERT、SET、ORDER BY、表和 JOIN 等语法结构 |
| `src/test/parser/parser_test.cpp` | 检查 SQL 是否生成了正确的 AST |

其他文件先知道用途即可：

| 文件 | 作用 |
| --- | --- |
| `parser.h/.cpp` | 对外提供 `Parse()` 和错误诊断 |
| `parser_internal.h` | Flex 和 Bison 之间传递中间值 |
| `CMakeLists.txt` | 生成并编译 Flex/Bison 代码 |

当前代码中的“支持”分为三层，不要混在一起理解：

| 状态 | 例子 |
| --- | --- |
| 已进入现有查询流程 | 基础 SELECT/INSERT/UPDATE/DELETE、AND 条件、单列 ORDER BY |
| Parser 能记录，Analyzer 会明确拒绝 | UPDATE SET 右侧的列或函数表达式 |
| 只有 AST 扩展位置，Grammar 尚未接入 | GROUP BY、HAVING、OR、NOT、LIMIT、JOIN ON、SELECT 聚合和子查询 |

预留 AST 的目的只是避免以后推翻数据结构，不表示学生现在需要实现这些功能。

推荐的阅读顺序是：

```text
parser_test.cpp
→ parser.h
→ lex.l 中的一条规则
→ yacc.y 中的一条 SELECT 规则
→ 对应的 AST 节点
```

不要修改构建目录中的 `lex.yy.cpp`、`yacc.tab.cpp` 和 `yacc.tab.h`。它们是自动生成的，下次构建时会被覆盖。

## 3. 怎样看懂 lex.l

Flex 规则基本长这样：

```text
匹配模式    { 匹配成功后执行的 C++ 代码 }
```

固定关键字不需要携带数据：

```text
"SELECT" { return SELECT; }
```

标识符需要把文字传给 Bison：

```text
{identifier} {
    yylval->text = yytext;
    return IDENTIFIER;
}
```

这里：

- `yytext` 是本次匹配到的文字；
- `yylval` 用来向 Bison 传递数据；
- `text` 定义在 `parser_internal.h` 的 `SemanticValue` 中。

Flex 会优先选择能够匹配最长文本的规则；长度相同时，写在前面的规则优先。初学者最需要记住：

1. 关键字要写在 `{identifier}` 前面，否则 SELECT 可能被当成普通名字；
2. `>=` 等多字符运算符虽然会因为“最长匹配”优先于 `>`，仍建议放在单字符运算符前面，便于阅读和维护。

本项目的关键字不区分大小写，因此 `select` 和 `SELECT` 使用同一条规则。

## 4. 怎样看懂 yacc.y

看一个当前项目中的规则：

```yacc
column_reference:
        table_name '.' column_name
    {
        $$ = std::make_shared<Col>(std::move($1), std::move($3));
    }
    |   column_name
    {
        $$ = std::make_shared<Col>("", std::move($1));
    }
    ;
```

它表示列引用有两种写法：

```sql
student.id
id
```

先记住三个符号：

- `$1` 是右边第一项的值；
- `$2` 是右边第二项的值；
- `$$` 是左边最终得到的值。

在 `table_name '.' column_name` 中：

```text
table_name '.' column_name
    $1      $2      $3
```

所以创建 `Col` 时使用 `$1` 和 `$3`。

Bison 还需要知道结果放在 `SemanticValue` 的哪个字段：

```yacc
%type <column> column_reference
```

这表示 `column_reference` 使用 `SemanticValue::column`。

### 什么是 epsilon

`%empty` 表示“什么都不写也合法”，也就是 epsilon 产生式。例如：

```yacc
optional_where:
        %empty
    {
        $$ = nullptr;
    }
    |   WHERE predicate
    {
        $$ = std::move($2);
    }
    ;
```

因此下面两条 SQL 都合法：

```sql
SELECT * FROM student;
SELECT * FROM student WHERE id = 1;
```

## 5. 第一次新增语法

假设想让 Parser 认识一条新的语句：

```sql
SHOW VERSION;
```

下面用它说明修改顺序。这个例子重点是学习流程，不表示当前系统已经实现 SHOW VERSION。

### 5.1 先写出 token 和 AST

期望 token：

```text
SHOW VERSION ';'
```

期望 AST：

```text
ShowVersionStmt
```

如果还说不清期望 AST，先不要开始写 Grammar。

### 5.2 设计 AST

这是一条新的完整语句，所以需要检查：

```text
ast.h：StatementKind 是否需要新类别，以及是否需要新的语句节点
```

AST 只保存语法信息。不要在 AST 构造函数中查询数据库，也不要保存执行器或查询计划。

### 5.3 让 Lexer 认识 VERSION

通常需要：

1. 在 `yacc.y` 中声明 VERSION token；
2. 在 `lex.l` 的 `{identifier}` 规则之前添加 VERSION 关键字规则；
3. 重新构建，先解决 token 拼写和声明错误。

固定关键字没有额外数据，不需要为它增加 `SemanticValue` 字段。

### 5.4 让 Grammar 接受 SHOW VERSION

SHOW VERSION 属于辅助语句，应接到 `utility_stmt`，并在语法动作中创建前面设计的 AST 节点。

语法动作只构造 AST，不检查数据库状态。

如果新增非终结符需要传递数据：

1. 在 `SemanticValue` 中增加或复用字段；
2. 使用 `%type <字段名>` 声明；
3. 用 `$1`、`$2` 读取右侧值；
4. 把最终结果写入 `$$`；
5. 不再使用的字符串、vector 和 `shared_ptr` 尽量用 `std::move` 交给 AST。

### 5.5 先写 Parser 测试

至少检查：

- 合法 SQL 可以解析；
- `statement->kind()` 正确；
- 少关键字或少分号时解析失败；
- 小写关键字也能解析。

走到这里，只能说明 Parser 完成了。

### 5.6 再处理 Analyzer 和 Planner

新增 `StatementKind`、`ExprKind` 或 `FromNodeKind` 后，搜索所有相关 switch：

```bash
rg "StatementKind" src
rg "ExprKind" src
rg "FromNodeKind" src
```

后续模块必须选择：

- 真正实现语义检查和执行；或者
- 明确抛出 `NotImplementedError`。

不要静默忽略新节点。

## 6. 不同语法通常要改哪些文件

### 新增关键字或简单拼写

```text
lex.l → yacc.y → Parser 测试
```

### 新增表达式

```text
ExprKind → expression.h → lex.l/yacc.y
→ Analyzer → Planner/Execution → 测试
```

UPDATE 的 SET 项采用一个容易扩展、也容易讲清楚的约束：

```text
SET 左侧：column 或 table.column
SET 右侧：scalar_expression
      ├── 字面量
      ├── column 或 table.column
      ├── 括号表达式
      └── 函数调用，例如 ABS(score)、SUM(score)、COUNT(*)
```

Parser 能生成表达式 AST，不代表执行层已经实现该表达式。例如函数出现在
SET 右侧时，Analyzer 当前会明确报告尚未支持，而不是把它错误地当成普通值。

OR、NOT 等逻辑表达式还要考虑优先级。通常 NOT 高于 AND，AND 高于 OR。随手追加规则可能产生错误的树或 Bison conflict。

### 新增 SELECT 子句

例如 GROUP BY、HAVING、LIMIT：

```text
SelectStmt 是否已有字段
→ ast.h 是否需要新的子句结构
→ lex.l/yacc.y
→ Analyzer
→ Planner/Execution
→ 组合测试
```

除了单独测试，还要测试它与 WHERE、ORDER BY 等子句组合后的顺序。

### 新增完整语句

```text
StatementKind/ast.h → lex.l/yacc.y
→ Analyzer → Planner → Portal/服务端 → 测试
```

## 7. Parser 应该做什么，不应该做什么

Parser 应该：

- 忠实保存 SQL 的语法结构；
- 拒绝语法错误；
- 保存准确的错误位置；
- 每次 Parse 使用独立的 AST 和错误状态。

Parser 不应该：

- 检查表和列是否存在；
- 完成列绑定或类型检查；
- 根据索引决定执行方式；
- 为了方便某个 Executor 而丢失 SQL 结构。

例如 FROM/JOIN 在 AST 中是一棵递归树。Planner 可以把它变成左深执行计划，但 Parser 不应提前把它压平成简单表名数组。

还要注意：AST 中已经有 GROUP BY、HAVING、LIMIT、OR、NOT、函数和子查询相关结构，不等于数据库已经支持这些 SQL。一个功能真正可用需要走完：

```text
Lexer → Grammar → AST → Analyzer → Planner → Execution → 测试
```

## 8. 构建和测试

第一次构建：

```bash
cmake --preset debug
```

修改 Parser 后：

```bash
cmake --build --preset debug -j 4 \
  --target framework_parser_test framework_query_planning_test rmdb
```

运行相关测试：

```bash
ctest --preset unit \
  -R 'framework_(parser|query_planning)_test' \
  --output-on-failure
```

Parser 测试位于：

```text
src/test/parser/parser_test.cpp
```

Parser 与 Analyzer/Planner 的联动测试位于：

```text
src/test/query/analyze_planner_test.cpp
```

## 9. 常见问题

### unexpected IDENTIFIER

新关键字可能没有写进 `lex.l`，或者关键字规则位于 `{identifier}` 后面。

### `$1 has no declared type`

检查对应 token 或非终结符是否通过 `%token <...>`、`%type <...>` 指定了 `SemanticValue` 字段。

### SemanticValue 没有某个成员

检查 `parser_internal.h`、`lex.l` 和 `yacc.y` 是否使用了同一个字段名，然后重新构建生成代码。

### Parser 测试通过，但 Analyzer 报 NotImplementedError

说明语法层已经接入，但后续语义或执行层还没有支持该节点。

### 出现 shift/reduce conflict

先检查表达式优先级、多个可选规则是否共享前缀、epsilon 分支是否过多。不要通过关闭 Bison 警告来掩盖冲突。

### 第二次 Parse 得到了上一次的 WHERE

不要把语句节点、可选子句或错误保存在全局变量中。每次解析的数据都应属于本次 `ParseContext` 和 AST。

## 10. 提交前检查

- [ ] 能画出目标 SQL 的 token 序列。
- [ ] 能画出期望 AST。
- [ ] 没有修改 Flex/Bison 生成文件。
- [ ] 新关键字位于标识符规则之前。
- [ ] Grammar 动作只构造 AST，没有查询数据库。
- [ ] 新增 Kind 后检查了相关 switch。
- [ ] 尚未支持的后续语义会明确报错。
- [ ] 合法、非法、可选和大小写情况有测试。
- [ ] `framework_parser_test` 已通过。
- [ ] 涉及后续层时，`framework_query_planning_test` 也已通过。
