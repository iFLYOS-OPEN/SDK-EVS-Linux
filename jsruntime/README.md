### iFLYOS js运行环境

基于 `https://github.com/pando-project/iotjs` 的 df12fd6c9d36d0bcfeb1ec954bf2164f2aa816d4, 增加了需要使用iFLYOS和实现通用只能音箱需求相关的native绑定

### cmake 参数

```
-DCMAKE_TOOLCHAIN_FILE=/home/jan/tmp/iotjs2/cmake/config/x86_64-linux.cmake 
-DCMAKE_BUILD_TYPE=Debug 
-DTARGET_ARCH=x86_64 
-DTARGET_OS=linux 
-DTARGET_BOARD=None 
-DENABLE_LTO=OFF 
-DENABLE_MODULE_NAPI=OFF 
-DENABLE_SNAPSHOT=OFF 
-DEXPOSE_GC=OFF 
-DBUILD_LIB_ONLY=OFF 
-DCREATE_SHARED_LIB=ON 
-DFEATURE_MEM_STATS=OFF 
-DEXTERNAL_MODULES='' 
-DFEATURE_PROFILE=es2015-all 
-DMEM_HEAP_SIZE_KB=4096 
-DEXTRA_JERRY_CMAKE_PARAMS='-DFEATURE_CPOINTER_32_BIT=ON' 
-DFEATURE_JS_BACKTRACE=ON 
-DEXTERNAL_LIBS='' 
-DEXTERNAL_COMPILE_FLAGS='' 
-DEXTERNAL_LINKER_FLAGS='' 
-DEXTERNAL_INCLUDE_DIR=''
-DIFLYOS_PRODUCT=product
```

``-DIFLYOS_PRODUCT=product` product要换成你的产品名称，会体现在js全局变量iflyosProduct中。

非x86 linux平台,比如 amlogic a113x, 需要修改参数

```
-DCMAKE_TOOLCHAIN_FILE=/home/jan/tmp/iotjs2/iflyosBoards/a113x/toolchain.txt
-DTARGET_ARCH=arm
``` 

所有交叉编译所需的编译链和第三方依赖库,建议统一放到`/etc/iflyosBoards`下面,否则需要根据本地路径修改`toolchain.txt`文件里面的路径

开发阶段建议 `-DCMAKE_BUILD_TYPE=Debug`,否则很多错误提示不全
