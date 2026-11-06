# B2 Camera 静止短窗口：最终联合验收

日期：2026-11-06（北京时间）。**B2 已通过本次批准范围内的完整验收：独立 B2 构型、静止真实 Camera、受限短窗口、Camera/SD 并行观测、正常关闭、全量 PC 回读重建。** 不代表长期持续记录、生产控制并行或车辆运动已验收。B1 为用户跳过，不计 PASS；B3/C1/D/E 未启动或放行。

## 验收对象与核心证据

- 固件：RECORDER_CAMERA_B2_STATIC_1，export=1、metrics=2；只有Capture/Writer/Monitor和Idle，无执行器/Encoder/车辆控制任务。
- 构建工程：C:/shengchanli/smartCar/smartcar/.build/recorder-camera-stages-20261106/run-20261105T202447-47412-final/candidate.uvprojx。
- 原构建：Keil ARMCLANG6.21，0错误0警告，196源/头/库/工具输入冻结；MAP无车辆业务及qsort。
- 本轮对应镜像快照：evidence/b2-tested-image-20261106/recorder_b2.axf、recorder_b2.bin；版本差异见下文。
- 板端原始日志与测量：evidence/b2-hardware-20261105T202845487Z/。
- PC原始下载和所有重建数据：C:/shengchanli/smartCar/b2_real_session_20261105T202916265Z/。
- 联合审核：evidence/b2-final-acceptance.json，15项配对/一致性检查全部true，b2_stage_accepted=true。离线工具和单独测量报告保持各自保守的hardware=false标志，由联合报告独立作阶段裁决，未篡改底层报告。

## 本轮真实结果

| 项目 | 结果 |
| --- | --- |
| 卡内目录 / Session | 2:/R0000003 / rec-599e5b78342531d7-0f162f3f-00000003 |
| 真实来源 | synthetic=false；抽样图像可见稳定室内场景 |
| 帧数 / 序号 | 101帧，37–137，逐帧可追溯到源文件偏移和捕获时间戳 |
| 输出规格 | 188×120，GRAY8，stride188，每帧22560 B；所有101张PNG尺寸及原像素一致 |
| CSI寄存器 | 宽度周期240、高度94；8-bit、1 B/pixel；240×94=188×120=22560 B |
| 帧间隔 | 最小19ms，中位数/P95/最大20ms；严格递增 |
| 记录时间跨度 / 有效帧率 | 首末跨度1,999,000us，100个间隔，约50.0250 FPS |
| 时间语义 | CSI完成IRQ的GPT1毫秒计数×1000，分辨率1ms；不是曝光开始时间 |
| 记录/同步/关闭 | observed=accepted=written=synced=101，state4，正常END与最终manifest，已卸载 |
| 丢帧/错误 | known_missing=0，unknown_source_loss=false，queue_full=pool_full=ambiguous=storage_errors=0 |
| 队列峰值 / 复制最大耗时 | 33/64；Capture复制路径最大2217us |
| 实际SD配置读回 | USDHC1，1-bit，clock_hz=23408866（约23.409MHz）；上限请求25MHz不能当实际25MHz |
| CRC / 文件 / PNG | 段头、metadata、record、payload CRC与END/manifest一致性全通过；逐帧源字节SHA和PNG无损往返通过 |
| 下载字节 / I/O统计字节 | 000000.scraw 2284489 B + session.json 921 B = 2285410 B，与板端累计f_write字节完全一致 |
| 标注抽样 | 每10帧抽样，11条Label Studio任务；全部101帧和原始源文件保留 |

100事件/约3秒为软件软停止策略：Monitor检查与Capture确认之间可多接收事件，本轮101帧不是丢帧或计数不一致。此次真实观察跨度约2秒，正常走请求停止、drain、sync、close；不声称严格100帧或硬实时停止上界。

CSI的240×94是zf_driver_csi.c::csi_init为188不能被8整除而选择的总字节数不变的DMA分组，并非把输出图像改成240×94。寄存器、驱动规则、22560 B帧长度和188×120图像重建相互一致；没有将metadata打印值单独当成传感器规格读回。传感器内部曝光/gain寄存器读回不可用，actual_exposure_readback保留null。

END的synced_before_end=69为写END之前的同步快照；最终板端同步计数101，END段与最终manifest正常关闭后才发布state4。该快照不表示余下32帧未同步。

## CPU、IRQ、任务栈与Writer延迟

任务runtime窗口从调度器启动累计至关闭后的报告快照，总计4,171,466us，覆盖挂载/记录/排空。CSI/SD IRQ累计包括预记录初始化及排空阶段，不等于101个记录事件。

| 项目 | 实测 |
| --- | ---: |
| CPU nonidle任务占用估计 | 39.9% |
| Idle / Capture / Writer / Monitor runtime | 2507125 / 312866 / 1296361 / 45651 us |
| CSI IRQ次数 / 最大耗时 | 208 / 3us |
| USDHC IRQ次数 / 最大耗时 | 1193 / 7us |
| Capture / Writer / Monitor / Idle 栈余量 | 996 / 3540 / 868 / 246 words |
| 完成排序与打印后的Monitor最小栈余量 | 846 words（3384 B） |

CPU估计基于FreeRTOS任务runtime，IRQ时间归入被中断任务；它不是周期级精确总CPU利用率。IRQ最大值含更高优先级抢占，不是WCET；栈余量只证明本次实际路径，不保证所有输入。CSI IRQ优先级2不调用RTOS等待，USDHC优先级5配合现有RTOS信号量；DMA缓冲经MAP核对分离且非缓存，采集与Writer没有共享正在写入的帧槽。

| API | 样本数 | P50 | P95 | P99 | 最大 | 累计 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| f_write | 309 | <1us* | 21.583ms | 25.322ms | 36.939ms | 1975.550ms |
| f_sync | 4 | 154.859ms | 157.008ms | 157.008ms | 157.008ms | 479.281ms |
| f_close | 2 | <1us* | <1us* | <1us* | <1us* | 0us* |

*原始整数微秒报告为0，表示DWT差值换算后的向下取整值，不代表物理零延迟。close发生在显式sync之后。分位数为最近秩，包含40 B记录头、22560 B像素和4 B尾部等不同大小调用，不能与64KiB块延迟直接逐项比较。所有API错误和样本溢出均0；每类最多512样本，统计哨兵与报告后栈余量检查通过，完整END标记已收到。

本轮累计字节除以write调用耗时约1129.73KiB/s；除以write+sync调用耗时约909.16KiB/s。后者是调用耗时口径，不是整段墙钟吞吐；均含文件系统处理和任务抢占，不是纯卡内部性能。队列在记录期出现明显积压，停止后已排空，不能据短窗成功宣称长期50FPS无丢帧。

## CSI/SD并行与历史SD-only对照

| 项目 | 历史SD-only 24.75MHz | 历史SD-only 49.5MHz | 本次Camera+SD |
| --- | ---: | ---: | ---: |
| CPU配置 | 600MHz | 600MHz | 600MHz |
| SD实际时钟 | 24.75MHz | 49.5MHz | 23.409MHz |
| 负载 | 16MiB、256次64KiB写、预分配 | 同左 | 101帧，记录头/像素/尾部分开写，间歇sync，无预分配 |
| f_write平均调用吞吐 | 1415KiB/s | 2095KiB/s | 1129.73KiB/s |
| 含同步等 | 1275KiB/s，含处理+最终sync墙钟 | 1809KiB/s，同左 | 909.16KiB/s，write+sync调用耗时口径 |
| 最大单次写调用 | 1082.646ms，64KiB | 1040.451ms，64KiB | 36.939ms，混合大小调用 |
| 本次数据校验 | 通过 | 通过 | 全部101帧通过 |

历史依据：demo/sd_performance/evidence/performance-hardware-20261105T163728Z/PERFORMANCE_REPORT.md及其原始serial.log。总线时钟、请求大小、预分配、同步频率、内存/驱动和计时口径不同，此表只呈现实际观测，不把差异归因为CSI或时钟，不构成matched-load因果对照。B1长期同负载压测按用户跳过，持续能力仍未验收。

## 三条真实Session与故障保留

| 目录 | 帧数 | PC完整性 | 仪表化状态 |
| --- | ---: | --- | --- |
| R0000001 | 100 | complete；CRC/序号/PNG通过 | 原始短窗，没有完整CPU/IO指标 |
| R0000002 | 100 | 本轮补回读complete；CRC/序号/PNG通过 | 旧metrics=1在关闭后的统计排序中出现损坏并失去响应，测量仍FAIL |
| R0000003 | 101 | complete；CRC/序号/PNG通过 | metrics=2报告、哨兵、栈余量及15项联合核查通过 |

301帧原始数据和三个SD目录均保留。R0000002回读通过不能把它的损坏性能统计翻成PASS。故障反汇编、MAP、原始日志与分析保留于evidence/b2-hardware-20261105T201919543Z/REPORT.md。v2采用固定空间有界排序，MAP无qsort；512相同值/逆序、计数越界、统计哨兵、缺END等Host检查通过。本轮实板观察到修复后的完整报告和846 words余量，但不把一次通过说成任意输入的运行时保证。

本轮图像10帧抽样已直接查看：室内场景结构连续，无黑帧或明显撕裂；全部101帧数值与PNG已校验。相邻帧完全重复对0；像素范围60–255、均值224.390–224.469，整体偏亮且有饱和区域。没有调整或美化图像，曝光标定和训练数据质量没有据此放行。图像预览在PC目录pc/review/contact-sheet.png，逐帧统计见pc/pixel-audit.json。

## 镜像、工程与工作区保护

当前AXF相对最初交付哈希发生变化，mtime在用户换烧前后；从AXF重新提取的68096 B负载与保留的原BIN仅有编译时间04:25:03→04:28:01的2字节不同，其他全部负载相同。Session metadata也报告04:28:01，196个冻结输入仍逐一一致。这与同源重编译相符，但未直接记录用户的构建操作，也未通过调试器逐字节读回Flash，不声称具有设备镜像独立证明。

已保留当前AXF/BIN快照与原交付BIN、MAP、日志及输入清单，不回写或覆盖旧产物。证据为evidence/b2-tested-image-provenance.json。

- 本轮对应AXF SHA256：4b90048d93a1e277159f65226e01bf88fd06690a5171bd3da2779dbecb9b10af。
- 本轮负载BIN SHA256：0772b2f7db1b676d92027a59097794cf97ca3a03fbfdc9f2561b33c81c70b21a。
- 两个新的下载目录分别为b2_real_session_20261105T202916265Z（R3）和b2_real_session_20261105T203027653Z（R2），在仓库外、工作区内；均使用CreateNew，不覆盖既有文件。

1060项初始源码基线无缺失。任务修改的三个既有文件仍为recorder_storage.c、archive.py、cli.py；主线C31/C26代码、参数、原生产及训练Keil工程与基线一致。两个旧SD工程.uvoptx相对最初基线另有变化（此前已记录，原因不作无证据归因），保留现状，不回退可能的用户Keil操作。详见evidence/b2-final-preservation-review.json。没有修改ACL、Git历史、全局配置或技能。

## 验收边界与下一阶段

1. 具备重新烧录条件，独立Keil构建及当前静止短窗已通过；本轮对应镜像已由用户换烧并完成上述实测。
2. 当前结束状态：文件已同步关闭并卸载，CSI接收停止；COM14已恢复115200并关闭，未使用COM10。相机曾初始化的status字段camera_started=1不表示仍有CSI DMA记录。
3. 没有代理烧录/复位、格式化、热插拔、删除/覆盖SD文件或执行器动作。未进行突然断电试验；当前板载FatFs回读不是PC读卡器独立文件系统路径。
4. B2通过范围仅为已批准的静止短窗。长期50FPS稳定性、CPU精确WCET、生产控制deadline、Encoder/PI/最终PWM、C26 OFF行为与车辆运动均未由本轮验证。
5. 若继续B3，必须先按任务书完成主线控制并行的静止回归、C26 OFF/ON与停车行为、Encoder/PI deadline及本车参数检查，并另获人工运动授权。本轮不继续任何车辆测试。

## 可复核命令与证据层

联合审核可复现为（输出必须使用新文件名）：

```powershell
python tools/finalize_b2_acceptance.py --board docs/changes/recorder-camera-stages-20261106/evidence/b2-hardware-20261105T202845487Z --download C:/shengchanli/smartCar/b2_real_session_20261105T202916265Z --build .build/recorder-camera-stages-20261106/run-20261105T202447-47412-final --provenance docs/changes/recorder-camera-stages-20261106/evidence/b2-tested-image-provenance.json --output <新的审核JSON文件>
```

JSON/XML/CSV及源码为structured_parsed；CRC/哈希、分位数、帧率、源文件/PNG往返、Host/Keil/MAP和反汇编为computed；10帧抽样PNG已直接视觉核验。无md_extracted/pdf_verified来源。实际曝光寄存器、长期稳定性和生产控制并行保留UNKNOWN或未验收，不使用假值填补。
