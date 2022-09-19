## OwnBase
OwnBase是rucbase项目中参与者构建自己的db程序的子项目。

### 使用方法
1. 完成对应lab,通过编译和测试
2. 在rucbase主目录`/build/lib`下找到自己实现的模块的对应`.lib`文件,替换`/ownbase/lib`下对应文件
3. 
    ```bash
    cd ownbase
    mkdir build
    cd build
    cmake ..
    make
    ```
4. 你将获得对应模块功能由自己实现的`yourbase`可执行文件,可以用`rucbase_client`进行连接测试