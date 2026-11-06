# B2 Camera Session：软件准备与实板验收状态

**软件准备完成；真实Camera连续Session尚未验收。** B1按用户跳过，不视为通过。

## 已交付

1. 原SCRAW-1 PC全序列重建、损坏恢复、连续帧/时间线/缺口报告、Label Studio抽样，保留全部原始字节。
2. 原archive修复END边界一致性。25项Recorder/B2测试通过（原17+新增8），另33项既有PC pipeline回归通过。B2窗口/命令策略Host测试通过，覆盖零时刻、时间回绕、阈值、CRLF与超长命令丢弃。
3. 真实C序列化器产生并保留四个合成示例：complete=3帧；gap/corrupt/truncated各恢复2帧，后三者均incomplete。全部synthetic=true，不能冒充B1吞吐或B2相机数据。
4. 新隔离构型：demo/recorder_b2/mdk/recorder_b2.uvprojx；原Recorder/Camera/SD/RTOS模块复用，只有Capture4、Writer2、Monitor5。未链接PWM/Encoder/Control/主线启动函数。旧生产工程/宏不变。

## 最终构建

.build/recorder-camera-stages-20261106/run-20261105T192407-50452-final/：Objects/recorder_b2.axf、recorder_b2.bin，Listings/recorder_b2.map，keil-build.log、inputs.json、build-evidence.json。ARMCLANG6.21，0错误0警告。Code43848、RO8604、RW188、ZI1826304B；非缓存帧/SD数据1664448B，DTCM83788B。冻结190个依赖，最终编译.d与冻结清单相符。构建记录见evidence/b2-build-evidence.json；复现脚本调用已成功，日志见b2-repro-build.log。

嵌入firmware_input_tree_sha256覆盖已冻结源文件/头/库与配置，identity自身排除以避免循环，完整生成内容保留。私有Camera/SD适配器从已有Recorder按哈希复用，没有改原SDK源文件；Camera SCCB静态库不透明，已纳入哈希。
候选编译选择RECORDER_VOLUME=2:、RECORDER_FIRMWARE_BUILD=RECORDER_CAMERA_B2_STATIC_1和动态stop_reason。原训练默认0:/RECORDER_TRAIN_1保持不变，该旧构型盘号缺口未被偷偷重定义。

## 候选的启动/停止

上电只初始化必要clock/GPT/UART，等待精确 `B2 START`；没有自动Camera/SD记录，更没有控制/执行器初始化。实际硬件操作尚未授权，本轮未烧录、未串口通信。
取得人工批准后：应先隔离电机/舵机供电、固定车辆并核对引脚与SD现有文件；只烧录本候选并观察ready，另行批准Camera+SD测试后才发送B2 START。不能用既有recorder_train替代本候选。
开始后Camera配置按188×120/50FPS初始化，真实规格需读回/测量。记录期间可发送B2 STOP；自动在记录约3s、100个观察事件或Monitor开始约10s请求停止，随后原Writer执行drain/sync/close。阈值是软件软停止，不是严格帧数/墙钟保证；ISR歧义与调度延迟可带来偏差，原SDK轮询也仍可能阻塞。
完成后只在新Rxxxx目录保留数据；已有文件不覆盖。记录状态4、safe_to_remove只代表输出构型抑制与文件系统关闭，不能证明物理静止。文件全量导出/PC只读重建后，才能判断此次Session完整性。
候选为Camera+SD-only，不与车辆控制并行，也不声称C26控制时序已验证。它仍复用Capture的vision copy缓冲，但没有Vision/Control任务和算法，额外拷贝开销需实测。

## 待验收 / UNKNOWN

真实相机宽高、位深/stride读回、帧率、曝光、CSI/SD总线争用、CPU与IRQ占用、栈高水位、停机drain实际时长、稳定DMA ownership/cache一致性、完整真实Session、与C26静止控制并行对照均未测。候选默认SD最高25MHz，不能套用旧SD-only49.5MHz性能。
进入B3前必须真实Camera验收、主线Encoder/PI/最终提交静止回归、C26 OFF行为验证与人工运动授权；本轮无这些放行。

## 可复现PC命令

```powershell
python tools/vision_data.py import --data-root C:/shengchanli/smartCar/b2_archive --source <已完整导出的Session目录>
python tools/vision_data.py continuous --data-root C:/shengchanli/smartCar/b2_archive --session-id <session_id> --output C:/shengchanli/smartCar/b2_report --every 10
python tools/recorder_stage_demo.py --output C:/shengchanli/smartCar/新的合成示例目录
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_recorder_b2.ps1
```

数据根必须在Git仓库之外，输出必须新目录。重构建请使用脚本分配新run，不要对既有归档输出重复Build；已提供AXF不需要为烧录再次编译。import/continuous对incomplete返回非零；原始损坏数据仍归档。完整示例及每条原始源文件哈希见evidence/b2-demo-result.json。

## 2026-11-06 更新：短窗已执行，等待全量下载

板端100帧短窗结果见 evidence/b2-hardware-20261105T194852617Z/REPORT.md。原B2工程已增加只读目录导出，新候选、使用顺序、Host/Keil证据与未满足指标见 B2_SESSION_EXPORT.md；本文件前述NOT_RUN/旧构建为历史状态，不代表当前候选。完整B2仍待实板下载和逐项审核。
