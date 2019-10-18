# 小明的工具仓库

## 编译依赖

* cmake 3.2以上
* [cmake-modules](https://github.com/Rokid/aife-cmake-modules.git)

## 编译命令

```
./config \
    --build-dir=${build目录} \  # cmake生成的makefiles目录, 编译生成的二进制文件也将在这里
    --cmake-modules=${cmake_modules目录} \  # 指定cmake-modules所在目录
    --prefix=${prefixPath}  # 安装路径
cd ${build目录}
make
make install
```

### 交叉编译

```
./config \
    --build-dir=${build目录} \
    --cmake-modules=${cmake_modules目录} \
    --toolchain=${工具链目录} \
    --cross-prefix=${工具链命令前缀} \    # 如arm-openwrt-linux-gnueabi-
    --prefix=${prefixPath}             # 安装路径
cd ${build目录}
make
make install
```
### 其它config选项

```
--debug  使用调试模式编译
--build-demo  编译演示及测试程序
```

## 工具详情

### 1. rlog

log工具

**文档暂缺**

### 2. caps

序列化及反序列化工具

[详情](./caps.md)

### 3. clargs

命令行参数解析工具

**文档暂缺**

### 4. uri

uri解析工具

**文档暂缺**
