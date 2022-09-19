## ownbase 静态库目录

> 这里存放各个模块的静态库,你可以用自己实现的库替换





```bash
find ./lib/*.a -type f -print0 |xargs -0 md5sum>libmd5.txt
```

