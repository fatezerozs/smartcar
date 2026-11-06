# 2026-11-07 调试策略与串口证据索引

本目录重点保存调试结论、下一步策略、COM15/COM17串口快照与少量缩略图。原视频仅在本地保留，已从本分支当前目录移除；此前提交历史不改写。未修改控制代码、未烧录、未发车或复位。

## 视频观察索引（原片留在本地）

| 原始文件 | 时长/秒 | 预览 |
| --- | ---: | --- |
| `973cce786be2113fec8d7f59eab55105.mp4`（本地） | 18.07 | [约19帧缩略图](contact-sheets/973cce786be2113fec8d7f59eab55105.jpg) |
| `e99088012deeea1e26803b4f543ed1d2.mp4`（本地） | 12.07 | [约19帧缩略图](contact-sheets/e99088012deeea1e26803b4f543ed1d2.jpg) |
| `b461a73c5b82dd759bbd26b49c10821e.mp4`（本地） | 70.60 | [约19帧缩略图](contact-sheets/b461a73c5b82dd759bbd26b49c10821e.jpg) |
| `7a5eba0fbe841785060fb6e5b8b68a43.mp4`（本地） | 4.97 | [约19帧缩略图](contact-sheets/7a5eba0fbe841785060fb6e5b8b68a43.jpg) |
| `00e72352b941c6c913c966a4d24ce313.mp4`（本地） | 4.17 | [约19帧缩略图](contact-sheets/00e72352b941c6c913c966a4d24ce313.jpg) |

视频按用户提供顺序排列，不代表严格拍摄时间排序。缩略图是均匀抽帧，不能代替完整视频或逐帧验收。外部拍摄视频不等同车载摄像头原始输入。

## 串口

[serial-snapshot.zip](serial-snapshot.zip) 包含两个持续监听目录的全部现有分卷原始rx.bin、接收分块时间chunks.csv、连接事件events.jsonl、session.json与当时监听脚本。解压后保持目录与分卷顺序；原始字节是依据，文本解码时应保留错误标记。

[serial-manifest.json](serial-manifest.json) 逐文件给出快照长度、SHA256与快照时刻。活动文件按开始读取时的长度截取，监听未停止；这是逐文件快照，不是原子同步快照，快照之后新收到的数据不在此包内。接收日志可能包含乱码、复位和多个固件身份，不能宣称零丢失。chunks.csv尾部与活动bin不保证原子对齐，回放必须以各bin实际快照长度为上限。不能把重复LAST_STOP当作新的停车事件。

COM15已见RINGLOG2和RINGLOG3标记，必须按固件身份、开机epoch、C26行驶窗口分组；五段视频尚未与串口时间一一对齐，不得用某条保留停车记录直接解释指定视频。COM17原始内容同样保留，不把零碎字符冒充完整遥测。

## 下一次接续位置

**先读[下一轮调试执行方案](../../docs/changes/2026-11-07-night-ringlog/NEXT-STEPS.md)。**

先阅读[今日总结](../../docs/changes/2026-11-07-night-ringlog/README.md)和[上传后复核及改进方案](../../docs/changes/2026-11-07-night-ringlog/FOLLOWUP.md)。RINGLOG1/2/3源码与固件分别保存在releases下，不能混用MAP导出日志。

本次初看：前两段能驶入环形通道，随后停滞/有人搬车；70.6秒段保留连续直弯能力，不能宣称完整环岛通过。后续优先保护发车与普通循迹，分析环内到出口的当前路径连续性、出口资格及停车链路，再考虑提速。

详细CRC校验的旧停车RAM导出已经分别打包于RINGLOG2/runlog1-evidence.zip和RINGLOG3/runlog2-evidence.zip。本次串口包不冒充新的RAM或SD卡导出。

## 校验

[SHA256SUMS.txt](SHA256SUMS.txt) 覆盖本目录除自身之外的文件；serial-manifest另提供压缩包内部文件的字节校验。原视频不再列入当前目录或校验清单。
