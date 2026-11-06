# B2 + B3 联合静止候选交付 — 2026-11-06

## 当前结论

生产配置已按操作者明确要求写入：左 CPR=2335.8、前进原始符号 −1；右 CPR=2358.6、前进原始符号 +1。整圈原始记录和重复性限制见 ENCODER_RESULT.md、measurement-ledger.json。参数采用不等同于计量重复性通过。

联合候选固件 RECORDER_JOINT_STATIC_C26_1 已独立构建，0 Error / 0 Warning，MAP 和 468 个输入文件摘要检查通过。尚未换烧该候选，板端 B2+B3 联合回归为 NOT RUN。原板上 B3 镜像尚未改变，COM14 已关闭。

## 可打开的工程和产物

- 工程：`C:\shengchanli\smartCar\smartcar\.build\recorder-joint-20261106\run-20261106T065153-38176-final\candidate.uvprojx`
- AXF：`C:\shengchanli\smartCar\smartcar\.build\recorder-joint-20261106\run-20261106T065153-38176-final\Objects\recorder_joint_static.axf`
- BIN：`C:\shengchanli\smartCar\smartcar\.build\recorder-joint-20261106\run-20261106T065153-38176-final\Objects\recorder_joint_static.bin`
- MAP：`C:\shengchanli\smartCar\smartcar\.build\recorder-joint-20261106\run-20261106T065153-38176-final\Listings\recorder_joint_static.map`
- Keil 日志：`C:\shengchanli\smartCar\smartcar\.build\recorder-joint-20261106\run-20261106T065153-38176-final\keil-build.log`
- 输入与隔离证据：`C:\shengchanli\smartCar\smartcar\.build\recorder-joint-20261106\run-20261106T065153-38176-final\inputs.json`、`C:\shengchanli\smartCar\smartcar\.build\recorder-joint-20261106\run-20261106T065153-38176-final\build-evidence.json`
- 编译输入摘要：`6fe0b9ea632b45a6cde010ff966eae902d20f60dda2d873d8f87308ebd36049b`

AXF/BIN 为全新输出路径，未覆盖旧镜像。工程关闭自动下载和前后构建动作；本次只执行 Keil -b。代码 102768B、只读数据 13976B、RW 240B、ZI 1971304B。DTCM 区 206316/449536B，非缓存 SDRAM 1664448/2097152B，MSP 8192B、heap 1024B，均在链接区上限内。已备齐用于人工换烧静止测试的产物；没有恢复动力的验收结论。

## 构型与隔离

复用当前 C26 主控制实现及其 Encoder/PI/最终输出链。Control 优先级5，Capture4，Vision3，Writer2，Monitor1；原生 PIT1 仍在优先级0执行10ms采样和切断。Vision 保留主工程 track Bayes 提示与 C26 route 状态；队列携带 mode epoch，模式变化前的旧观测不能用于新模式。未链接 midterm_boot_poll、旧 Recorder Control、自启动入口或裸机 app_runtime_run。

仅私有适配 PWM init/set，两处都无条件把两个电机和舵机的实际 duty 强制为0，PI请求仍记录。SD 时钟/引脚一次性在 Camera 和控制前初始化，使用既有 1-bit/3.3V/25MHz 请求上限 SD 适配。未修改 Recorder、SDK 驱动或生产 Keil 工程；仅用户明确授权的生产参数与方向宏引用写入原源码，精确差异见 production-change.patch。

Writer 是 FatFs 唯一写入者，启动需命令授权。独立新建唯一 Session 和 CREATE_NEW 文件，不删除、覆盖、格式化或改分区。生产者硬上限100帧（图像净载荷最多2256000B），Monitor另设3秒停止请求；3秒为软期限，包含挂载/开文件的耗时，不是硬实时承诺。Drain 复用5秒超时和 SD 隔离。一次启动仅允许一个 Session；重测需人工复位或重新上电，本工具不操作复位。

## 板端流程（待人工换烧）

保持底盘固定、电机和舵机断电，相机、SD和COM14连接，C26 OFF。人工换烧上列 AXF 并运行。候选上电等待 `JOINT START`，不会直接创建 Session。

默认 `JOINT RUN` 对照用于 C26 OFF 情况；完整静止联合测试使用 `JOINT ARM`。PC 工具会先核对固件身份、编译输入摘要、强制零输出和 C26 OFF，再启动 Camera/RTOS。使用：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\shengchanli\smartCar\smartcar\tools\test_recorder_joint_static.ps1 -C26TransitionTest
```

1. Recorder OFF 基线5秒，Camera和原生控制持续运行。
2. 输出 `JOINT WAIT C26_ON` 后等待人工拨ON，最长60秒；本阶段不开始写文件。ON稳定后进入 Writer 窗口，同时验证原生C26逻辑、PI服务和有限写入。
3. Writer同步、关闭并卸载后输出 `JOINT WAIT C26_OFF`，再人工拨OFF；最长60秒。之后采集Recorder OFF收尾5秒。
4. PC工具保存逐行原始串口日志和采集结果；每次关闭COM14，DTR/RTS均关闭，不访问COM10、不下载固件或复位。

电机断电且轮子静止时，原生无计数保护可能终止逻辑运行。必须报告实际 PI 活跃采样数、故障位及 `JOINT LAST_STOP` 对应本窗口的时间/原因，不能绕过保护，也不能把 idle PI 调用算作有效闭环计算。程序打印的 PI consumption 只计目标非零且消费序列发生变化的更新。

分相位报告 Camera/Vision 帧数与年龄、10ms采样间隔、PI周期/IRQ掩码最大耗时、队列深度/峰值/丢帧、写卡错误、PWM请求/实际值/寄存器、Control服务间隔及10ms服务预算超限。该服务预算是本次回归的工程检查值，不替代原生50ms失联保护，也不宣称硬实时WCET。

文件独立回读未自动完成：闭卷且C26 OFF时可使用保留的 `B2 EXPORT LIST/INFO/READ/BAUD` 只读命令，随后必须PC逐块CRC及完整Session校验。旧 B2SessionExport 客户端的启动握手只识别B2固件，不能直接当作JOINT客户端使用。PC串口对照通过不会将 `sd_file_crc_verified_on_PC` 改为true。

## 验收状态

| 项目 | 状态 |
|---|---|
| 手转正反方向映射 | 两侧符合现有软件约定，原始证据保留 |
| CPR生产配置 | 已按用户指令采用10圈实测比值 |
| CPR计量重复性 | 未通过独立重复性确认，保留漂移/碰动记录 |
| Keil独立构建 / MAP / 输入摘要 | PASS，0错误0警告 |
| 原生C26 / PI / Encoder Host回归 | PASS，原断言不变 |
| 联合Control/Vision Host | PASS，OFF/ON、STOP、OFF再授权、队列覆盖、模式epoch |
| 相位统计Host | PASS，相位隔离、消费序列、超时/IO错误/非零PWM检测、计数环绕 |
| PC命令客户端 | 仅CheckOnly编译/解析自检PASS，未打开COM |
| 实际Camera + SD + C26联合 | NOT RUN，需人工换烧候选 |
| 实际SD文件PC CRC复核 | NOT RUN |
| 动力/赛道运动 | 未批准恢复，本阶段不执行 |

读取层：C/JSON/XML原生结构为 structured_parsed；实际二进制 CRC、模差/CPR与摘要、Host断言为 computed。圈数来自操作者。没有把模拟或构建成功当作硬件联合验收。

没有执行 git clean/reset/checkout/revert，没有改 ACL。没有烧录、复位、恢复电机或舵机供电。原有未提交修改保留；生产文件仅增加明确授权的 Encoder 参数和对应方向宏读取，备份及 SHA256 已交付。
