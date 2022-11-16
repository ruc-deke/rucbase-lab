##  lab3测试说明

在lab3中,系统目前不使用`GoogleTest`框架进行测试,而是采用`Linux Bash`脚本对你完成的各个任务进行`SQL`语句执行并比对正确的结果,你能够在脚本执行完成时看到通过测试与否的提示,如果未能通过,系统会使用`diff`将你的输出和正确输出进行比对方便你定位执行错误的地方。

- `output.txt` : 你的执行输出文件
- `res_output.txt`：正确的执行输出文件
- `input_*.sql`：各个任务点执行的SQL语句
- `task_test.sh`：测试脚本

测试脚本依赖二进制程序`exec_sql`，你可以按照下列语句进行编译:

```bash
cd rucbase;
cd build
cmake ..
make exec_sql
```

得到的`exec_sql`程序在`rucbase/build/bin/`目录下，请不要移动该程序。


### 其他事项

如果你无法执行`.sh`脚本，请执行以下命令，以`task1`为例：

```bash
chmod +x task1_test.sh
./task1_test.sh
```

注意：**不能**使用`sh task1_test.sh`运行脚本。


在不影响主要比对逻辑的情况下，你可以自行根据需求修改测试脚本。

