# B2 首次静止短窗口实板测试

测试时间：2026-11-06 03:48:52—03:49:05（北京时间；原始事件使用 UTC）。

用户批准静止 Camera 采集与短窗口测试，确认硬件固定及电机/舵机电源隔离，随后确认手动烧录完成。仅使用有线 COM14、115200、8N1，DTR/RTS=false。未访问 COM10，未烧录、复位或发送车辆控制命令。

## 实际操作与结果

1. B2 STATUS 返回匹配身份 RECORDER_CAMERA_B2_STATIC_1、rx=irq、camera_started=0、session_started=0。
2. 只发送一次 B2 START；未重试，未发送 B2 STOP，固件达到观察事件上限后自行停止。
3. 新目录 2:/R0000001；状态按 0→2（记录）→3（排空）→4（关闭/卸载）变化。
4. 终态 error=0、written=100、synced=100、observed=100、drop=0、pool=0、ambiguous=0、queue=32（高水位）。written/synced 按存储源码为帧计数，不是字节数。
5. START 到首次收到关闭状态约 4.34 秒，包括相机初始化、记录、排空与状态输出采样；不能作为精确采集时长或帧率。
6. 结果后关闭串口，保留所有文件。没有运行第二轮、持续压力测试或车辆运动。

## 验收边界

实板短窗口流程已执行且板端计数无报告错误；完整 B2 Session 尚未验收。当前仅有板端状态，尚未取得 Session 原始文件，不能证明每帧 CRC、像素内容、真实尺寸/曝光/帧率或断电持久性。queue=32 表明存在积压，需要结合原始时间线与后续争用测试评估，不能据此宣称长时间稳定采集。

当前 B2 候选没有 Session 目录导出命令；此前 SD Performance 导出只覆盖根目录诊断 BIN，不能导出本目录。下一步需通过 PC 读卡器只读复制完整 R0000001 目录，或为当前 B2 固件增加只读 Session 导出后手动换烧，再使用现有 import/continuous 逐帧验证。不要删除或覆盖本 Session。

证据：serial.bin 原始接收字节、serial.txt 可读日志、events.jsonl 收发时间、result.json 保守判定。JSON 与源码计数含义为 structured_parsed；时间差为 computed。硬件数据未做 PC CRC 验证；B3/C1/D/E 未启动或放行。
