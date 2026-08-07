# RMDB Wire Protocol v3.0

RMDB Wire 是 Rucbase 服务端、课程客户端和黑盒测试之间的稳定二进制协议。学生代码通常不需要处理协议细节；推荐直接使用 `net/client.h` 中的 `rucbase::wire::Client`。

协议中的多字节整数均使用网络字节序（big-endian）。一条连接同一时间只能执行一个请求，不支持流水线复用。

## 1. 握手

TCP 或 Unix stream socket 建立后，客户端首先发送 8 字节：

| 偏移 | 长度 | 内容 |
| --- | ---: | --- |
| 0 | 4 | ASCII `RUCB` |
| 4 | 2 | major version，当前为 `3` |
| 6 | 2 | minor version，当前为 `0` |

当前握手的完整十六进制字节为：

```text
52 55 43 42 00 03 00 00
```

服务端只接受完全匹配的版本，并原样回写这 8 字节。任何一方读到 EOF、超时或不匹配版本，都必须关闭连接。

## 2. 帧格式

握手完成后，双方交换以下格式的帧：

| 偏移 | 长度 | 内容 |
| --- | ---: | --- |
| 0 | 4 | payload 长度，不包含 8 字节帧头 |
| 4 | 1 | tag |
| 5 | 1 | flags，v3.0 必须为 `0` |
| 6 | 2 | reserved，必须为 `0` |
| 8 | N | payload |

单帧 payload 最大为 1 MiB。`ERROR` 和 `TRANSACTION_ABORT` 的诊断文本最大为 64 KiB。接收端必须先校验长度和 reserved 字段，再分配 payload。

## 3. Tag

### 客户端到服务端

| 名称 | 值 | payload |
| --- | ---: | --- |
| `EXEC_STREAM` | `0x20` | 非空 SQL 字节串，通常为 UTF-8 |

### 服务端到客户端

| 名称 | 值 | payload |
| --- | ---: | --- |
| `META` | `0x01` | 查询结果列定义 |
| `ROW` | `0x02` | 一行类型化数据 |
| `COMMAND_OK` | `0x10` | 必须为空 |
| `RESULT_END` | `0x11` | 8 字节无符号行数 |
| `TRANSACTION_ABORT` | `0x12` | 诊断文本 |
| `ERROR` | `0x13` | 诊断文本 |

## 4. 类型化结果

### 4.1 SQL 类型

| 类型 | 值 | 非 NULL 单元格内容 |
| --- | ---: | --- |
| `INT32` | `0x01` | 4 字节二进制补码整数 |
| `FLOAT32` | `0x02` | IEEE-754 binary32 的 4 字节 bit pattern |
| `CHAR` | `0x03` | 4 字节长度 + 对应字节 |

### 4.2 META

```text
u16 column_count
repeat column_count times:
    u16 name_length
    byte[name_length] name
    u8 sql_type
```

`column_count` 和 `name_length` 均不得为 0。编码器不得静默截断列名或把未知类型转换为 `CHAR`。

### 4.3 ROW

`ROW` 不重复携带列数和类型，而是严格按照之前 `META` 的列顺序编码：

```text
repeat META.column_count times:
    u8 present                 # 0 = NULL, 1 = 有值
    if present == 1:
        value according to META.sql_type
```

除 `0` 和 `1` 之外的 present 值非法。帧中不得包含未被列定义消费的尾随字节。

### 4.4 RESULT_END

payload 是一个 `u64 row_count`。它必须等于此前收到的 `ROW` 帧数量。

## 5. 请求状态机

每个 `EXEC_STREAM` 必须得到以下一种终止响应：

```text
COMMAND_OK

META -> ROW* -> RESULT_END

ERROR

TRANSACTION_ABORT
```

`ERROR` 或 `TRANSACTION_ABORT` 也可以终止已经发送 `META` 的结果。它们是完整的协议响应，因此连接仍保持同步，可以继续执行下一条 SQL。

未知 tag、非零 flags、帧顺序错误、截断帧或行数不一致属于协议错误；客户端必须将连接视为不可复用并关闭。

## 6. 教学扩展

客户端可发送以下 `EXEC_STREAM` payload 查询当前数据库名：

```text
__RUCBASE_DATABASE_NAME__
```

服务端以名为 `database` 的单列 `CHAR` 结果返回数据库名。该扩展用于交互式客户端提示符，不进入 SQL 解析器。

测试用 `crash` 控制命令默认关闭。只有服务端进程显式设置 `RUCBASE_ALLOW_TEST_CRASH=1` 时才启用。

## 7. 资源和超时约定

- 官方客户端默认连接超时 5 秒、单次 socket I/O 超时 120 秒。
- 服务端默认只监听 `127.0.0.1`；需要远程实验环境时可显式传入 `-b 0.0.0.0` 或其他地址。
- 服务端限制同时活跃的连接数，并在退出时 shutdown 所有会话、等待处理线程结束后再关闭数据库。
- 大结果应通过 `ExecuteOptions::on_row` 逐行消费并设置 `format_text = false`，避免在客户端保存完整文本。

## 8. 推荐调用方式

```cpp
#include "net/client.h"

rucbase::wire::Client client;
auto status = client.ConnectTcp("127.0.0.1", 8765);
if (!status.ok()) {
    // status.message
}

auto result = client.Execute("select * from student;");
if (result.ok()) {
    // result.text
} else {
    // result.diagnostic
}
```

`Client` 自动完成握手、串行化请求并在析构时关闭 socket。SQL 错误不会破坏连接；传输或协议错误会使客户端自动关闭该连接。
