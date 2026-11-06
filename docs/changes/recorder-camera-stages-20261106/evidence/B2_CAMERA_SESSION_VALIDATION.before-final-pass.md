# B2 Camera Session：实板与 PC 联合验收

更新：2026-11-06（北京时间）。结论：**真实静止 Camera 短窗的数据链路 PASS；完整 B2 尚待一次仪表化短窗，不标为全部通过。** B1 仍按用户跳过，不计 PASS；不放行 B3/C1/D/E 或生产集成。

## 真实 Session 与追溯

- 卡内目录：2:/R0000001；Session ID：rec-599e5b78342531d7-0f162f3f-00000001。
- 原始下载、逐帧图像、时间线和完整分析：C:/shengchanli/smartCar/b2_real_session_20261105T200435275Z/。
- 源文件为 000000.scraw（2,261,885 B）和 session.json（921 B），合计2,262,806 B。所有100帧保留，不以10帧标注抽样替代原序列。
- 捕获固件是 run-20261105T194348-27184-final；metadata中的 firmware_input_tree_sha256 与其冻结清单一致。下载使用后续加入只读导出的 B2 固件，不混淆采集版本与导出版本。
- 只使用 COM14；先验证冷启动 idle 身份，再进入只读SD模式。未发 START，未复位/烧录/修改SD文件。本次下载不是PC读卡器独立文件系统路径；换烧期间的复位也不等于已验证突然断电安全。

## 已通过的真实证据

| 项目 | 实际结果 | 证据 |
| --- | --- | --- |
| 板端采集/关闭 | observed=written=synced=100，error=0，state4 | evidence/b2-hardware-20261105T194852617Z/ |
| 非合成来源 | synthetic=false；图像可见真实室内场景 | 源manifest、全部PNG、pc/review/contact-sheet.png |
| 帧数量/序号 | 100帧，38至137，已知缺帧0，未知丢失false | pc/continuous/frame_timeline.csv、missing_frames.json |
| CRC和格式 | 头/metadata/record/payload及END边界校验通过，无issues | pc/archive/sessions/<session_id>/integrity.json |
| 完整性 | complete，包含正常END与最终manifest，无GAP记录 | pc/continuous/continuous_report.json |
| 原始像素/PNG | 逐帧源字节SHA与归档一致，PNG无损往返验证通过 | pc/pixel-audit.json |
| 输出几何 | 每帧22,560 B，188×120，stride188，GRAY8 | 原始帧记录、100张PNG尺寸；尚非传感器寄存器读回 |
| 采样时序 | 严格递增，间隔min19ms/median20ms/P95 20ms/max20ms | 完整timeline与continuous报告 |
| 有效记录帧率 | 99个间隔跨度1,979,000us，约50.0253 FPS | 根据CSI完成时间戳计算；1ms分辨率 |
| 缓冲/争用观测 | queue高水位32/64，pool_full=queue_full=ambiguous=0 | 板端日志、END统计 |
| 捕获复制 | capture_copy_max_us=2186 | END；DWT测量含本路径复制等工作，不是整任务CPU占用 |
| 抽样标注 | 每10帧抽样，10条Label Studio任务，全部原帧仍保留 | pc/continuous/label_studio/tasks.json |
| 传输 | 逐块目录/文件/偏移/长度/CRC核对通过；恢复115200并关闭COM14 | serial-export.log、acquisition.json |

END内 synced_before_end=69 是写END前的同步快照，不是最终只同步69帧；板端最终同步计数100，END段与最终manifest完成同步/关闭后才发布state4。不得将此快照误读为31帧未同步。

100帧相邻像素块没有完全相同的重复对。像素范围59–255，各帧平均值224.035–224.150，标准差41.334–41.521，灰度取值188–194种。抽样图像结构连贯、整体偏亮；这不证明曝光标定或全部像素正确，实际曝光读回仍null。没有修改曝光/图像内容以美化验收。

## 完整 B2 尚欠的指标及处理

原短窗未记录CPU任务运行时间、栈余量、Writer逐调用延迟分布、CSI/SD IRQ耗时及实际CSI几何/SD时钟寄存器读回。现有文件和串口状态无法反推这些数值。离线continuous工具故意保持 b2_hardware_accepted=false；联合审核结论另存 evidence/b2-combined-review.json，没有篡改该保守标志。

已完成同一B2工程的仪表化候选：

C:/shengchanli/smartCar/smartcar/.build/recorder-camera-stages-20261106/run-20261105T202447-47412-final/

含 Objects/recorder_b2.axf、recorder_b2.bin、candidate.uvprojx、Listings/recorder_b2.map、keil-build.log、inputs.json、build-evidence.json。Keil 0错误0警告，MAP仍无执行器/Encoder/Control业务符号，依赖冻结闭合。build-evidence复制见 evidence/b2-metrics-v2-build-evidence.json。

新增测量仅在B2启用：私有FreeRTOSConfig增加任务runtime统计；原Recorder配置文件未改。存储模块3个I/O wrapper增加默认无操作的可选hook，旧构型不启用。记录write/sync/close延迟P50/P95/P99/max、累计字节/耗时/错误及采样溢出，最多每类512个样本，溢出必须显式失败而非静默丢统计。正常关闭后停止CSI，打印绑定Session的测量，再允许只读导出。

CPU nonidle_share 是FreeRTOS任务runtime估计，IRQ时间会归入被中断任务，不能称周期级精确总CPU利用率；IRQ最大值包含高优先级抢占，不是WCET；I/O延迟包含任务抢占与文件系统处理，不是纯卡内部耗时。DWT每次I/O区间需小于32位cycle计数回绕周期，3秒I/O超时策略与当前短窗满足设计范围，但不建立任意阻塞的硬上界。

补测流程已准备：用户手动换烧该候选并Run后，使用 tools/test_recorder_b2_static.ps1 -RequireMetrics 只跑一次原有100事件/约3秒窗口；tools/review_b2_metrics.py 审核测量完整性；随后 tools/download_b2_session.ps1 -Session <此次新目录> 直接在closed状态导出，无需再次复位或烧录。旧R0000001保留。采集授权沿用用户已批准的静止短窗，不扩展时长或车辆动作。

软件验证：25项Recorder/连续Session回归PASS；计数回绕、分位数、和、错误计数、采样上限Host检查PASS；3项测量解析测试PASS，旧无metrics日志、采样溢出或错误几何不会被伪造通过。此前只读导出9场景与PC包装器3项结果保留。

## 源码与未提交内容保护

当前对1060项初始基线逐文件哈希复核，无文件缺失。三个预期业务文件变化为 recorder_storage.c、archive.py、cli.py；主线控制、参数及原生产/训练Keil工程与初始基线一致。

另发现旧 SD Diagnostic/Performance 的两个 .uvoptx 用户选项文件相对初始基线变化，修改时间分别为2026-11-06 04:02:40/43（北京时间），在本次下载前。本轮代码未编辑这两个文件，没有前像支持对其原因作最终归因，保留现状，不回退用户可能的Keil操作。完整差异名单见 evidence/b2-readback-preservation-review.json。不能宣称全部1060文件字节未变；可以确认本轮没有覆盖原生产工程或回退未提交内容。

## 证据层

原生Markdown/代码、XML/JSON/CSV解析为structured_parsed；CRC/哈希、图像往返、帧率、Host、Keil和MAP为computed；PNG抽样已直接视觉核验。没有md_extracted或pdf_verified来源。CPU/Writer/IRQ等缺失量仍为UNKNOWN，待补测后据真实数据更新最终B2裁决。

仪表化计数口径：任务runtime从调度器启动累计到关闭后的报告快照；CSI/SD IRQ计数包括预记录初始化与排空阶段，不等于Session的100帧事件。测量打印和固定空间排序在关闭后进行，避免将串口大量打印混入记录阶段。测量额外开销和私有RTOS trace本身属于本次实验构型，不据此宣称原构型周期级WCET。


## 仪表化首次实板失败与v2修复

2026-11-06 04:19:26（北京时间）发送一次START，生成R0000002。达到100观察事件后进入关闭后的测量报告，sync/close统计出现损坏数值并失去后续响应；本轮不通过，CPU/IRQ/Writer数字不用于最终验收。原始日志、拒绝报告和根因分析见 evidence/b2-hardware-20261105T201919543Z/REPORT.md。R0000002尚未PC回读；R0000001此前100帧通过结论保持。

v2移除递归qsort，改成最多512元素、固定栈占用的排序；加入统计区双哨兵、计数边界、报告后栈余量和END标记，并在统计前先报告真实关闭状态。主机只接受metrics=2，完整报告缺失或损坏必须失败。MAP确认无qsort，Host覆盖512个相同值和逆序、计数越界与哨兵损坏，已构建0错误0警告；v2尚未实板测试。

CSI读回240×94是现有zf_driver_csi对188×120/22560字节的8字节对齐分组，SDK配置bytesPerPixel=1、8-bit总线，二者乘积相同；此前解析器要求CSI寄存器直接等于188×120是解释错误，已依据驱动源码修正并保留旧证据。这不等于实际曝光读回。
