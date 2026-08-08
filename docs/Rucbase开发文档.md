# Rucbase开发文档

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->

- [flex && bison文件的修改](#flex--bison%E6%96%87%E4%BB%B6%E7%9A%84%E4%BF%AE%E6%94%B9)
- [代码规范](#%E4%BB%A3%E7%A0%81%E8%A7%84%E8%8C%83)
- [注释规范](#%E6%B3%A8%E9%87%8A%E8%A7%84%E8%8C%83)
  - [example](#example)
- [相关资料](#%E7%9B%B8%E5%85%B3%E8%B5%84%E6%96%99)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

## flex && bison文件的修改

Parser 只保留 `lex.l` 和 `yacc.y` 作为词法、语法的源文件。CMake 会在构建目录中调用 Flex/Bison 生成并编译 `lex.yy.cpp` 和 `yacc.tab.cpp`，不要手动生成到 `src/parser/`，也不要提交生成文件。

修改 `lex.l` 或 `yacc.y` 后，正常构建即可：

```bash
cmake --preset debug
cmake --build --preset debug -j 4
```

Parser 对外只提供 `parser.h` 中的 `Parse(sql)` 接口，调用者不需要了解 `yyparse` 或 Flex buffer。各文件的职责如下：

- `ast.h`：SQL 抽象语法树。
- `lex.l`：把 SQL 文本转换为 token。
- `yacc.y`：把 token 组合成 AST。
- `parser.h/.cpp`：统一的解析入口和错误诊断。
- `parser_internal.h`：Flex/Bison 共用的内部数据，其他模块不应包含该文件。

## 代码规范
> 以VScode format配置为例

```
{ 
    BasedOnStyle: Google, 
    IndentWidth: 4,
    TabWidth: 4, 
    ColumnLimit: 120
}
```

## 注释规范
使用`/** */`写注释块

使用`//` 写行内注释

使用`//*/`作为代码注释开关

尽量不要使用中文标点符号
注释块符号
```
@brief: 简要描述
@param: 参数描述
@return: 用它来制定一个 method 或 function的返回值
@note: 注意点
@see: 用它来指明其他相关的 method 或 function . 你可以使用多个这种标签.
```
### example
```cpp

/**
 * @brief: 初始化系统.
 * @note: 动态参数不可为0
 * @param: dynamParam 动态参数
 * @param: controlPrarm 控制参数
 * @return: true初始化成功, false初始化失败
*/
bool init(int dynamParam,int controlPrarm){
    //*/
        codeSegement1;  
    /*/
        codeSegement2;  
    //*/
}

```

## 相关资料
[GoogleTest框架简介](https://www.cnblogs.com/jycboy/p/6057677.html)
