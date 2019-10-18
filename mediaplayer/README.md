# 从 [yodaos](https://github.com/yodaos-project/yodaos)扣出来的播放器,接上 iFLYOS 的 IPC 

### 依赖:

`-lavformat -lavutil -lavcodec -lswscale -lavutil -lswresample -lavdevice -lpulse`

`wavplayer` 依赖 `libportaudio`

### 交叉编译

```
-DCMAKE_TOOLCHAIN_FILE=$PWD/jsruntime/iflyosBoards/a113x/toolchain.txt
```
* 安装
```bash
make DESTDIR=/etc/iflyosBoards/a113x/install install
```

动态库会在 `/etc/iflyosBoards/a113x/install/usr/local/lib` 下面．可执行文件在 `/etc/iflyosBoards/a113x/install/bin` 和　`/etc/iflyosBoards/a113x/install/usr/local/bin` 下． 