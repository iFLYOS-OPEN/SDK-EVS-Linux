此项目是在树莓派上接入iFLYOS的参考. 包括了完整的唤醒，交互，媒体播放等通用功能.

## 树莓派系统准备

* 购买并给树莓派安装raspbian:buster
* 配置好网络
* 配置alsa音频系统,设置默认输入输出设备. 如从usb外置声卡输入输出,参考:

```bash
$cat /proc/asound/cards
0 [ALSA           ]: bcm2835 -
1 [ArrayUAC10     ]: USB-Audio -
$echo 'defaults.pcm.card 1' | sudo tee /etc/asound.conf
$echo 'defaults.ctl.card 1' | sudo tee -a /etc/asound.conf
$sudo /etc/init.d/alsa-utils restart
$arecord -c1 -fS16_LE -r16000 test.wav #录音,ctrl+c结束
$aplay test.wav #应该可以听到刚才的录音
```
## 开放平台申请

https://device.iflyos.cn/device

## 开发环境

* 安装所需的工具和运行库

```bash
$sudo apt install -y cmake libcurl4-openssl-dev libmbedtls-dev libsqlite3-dev libasound2-dev libsox-dev libatlas-base-dev libavutil-dev libavformat-dev libavdevice-dev libsdl2-dev pulseaudio
```

* 下载此项目
```bash
$mkdir ~/project
$cd ~/project
$git clone https://github.com/iFLYOS-OPEN/SDK-EVS-Linux.git
```

* 编译此项目
```bash
$mkdir ~/evs-build
$cd ~/project/rpi
$./build-iflyos.sh -p rpi -c {CLIENT_ID} -d {DEVICE_ID} -o $HOME/evs-build 
```
CLIENT_ID为在开放平台申请账号时，系统生成提供.

DEVICE_ID为自定义，作为当前设备的唯一标识.

如果没有错误，编译结果在 `~/evs-build`下面.

## 运行

* APP注册

下载app端[小飞在线](https://cdn.iflyos.cn/docs/iflyhome.apk),注册用户.

到[iFLYOS设备接入控制台](https://device.iflyos.cn/products)选择你的`clientId`对应的设备,把你注册的手机号添加到白名单.

* 启动程序
```bash
$cd ~/evs-build
$./bin/evsCtrl.sh start
```

* 授权

在设备未授权的情况下，通过"snowboy"唤醒，会自动进入配网模式. 

在手机端打开小飞在线并保持手机蓝牙开启，进入添加设备界面，此时应能够识别到正处于配网模式的当前设备.

点击"连接此设备"后进入对设备的WiFi配置页面，填写WiFi信息后点击"下一步"，此时设备会报"正在努力联网".

授权成功以后，会听到"我已联网,你可以对我说xxxxx".

* 交互

用"snowboy"唤醒,然后可以开始对话

## 定制开发

如果要在iFLYOS SDK之外开发定制功能，请先查阅[iFLYOS设备接入文档](https://doc.iflyos.cn/device/)，确保了解iFLYOS的运行原理后，再开始。
