## 音效文件

* 音效源文件请在　[EVS设备](http://wiki.iflyos.vip/pages/viewpage.action?pageId=6949106)　的提示音章节里下载.

* 下载完解压到一个空目录后，根据 [evsSoundEffect.js](/src/evsSoundEffect.js) 的 `var sounds` 数组修改其中的中文文件名．

* 如果要转成raw pcm

```bash
for i in `ls *.mp3`; do
ffmpeg -i <input> -ac 1 -f s16le -ar 16000 -acodec pcm_s16le <outputdir>`echo $i | cut -d'.' -f1`.raw;
done
```
