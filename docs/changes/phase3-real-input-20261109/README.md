# Phase3：真实输入审计、20ms边界修复与V3板端验证

2026-11-09，北京时间。继续Phase3流程，不开发上位机，不追加现场标记点。V3已烧录、确认运行身份，并完成三次停机计算测试和一次动力隔离的逻辑ON记录。

## 已修复的软件阻断

`vps_tick`原先用`(float)dt * .001f`将整数毫秒转换为秒。IEEE单精度下，20ms的乘法结果略大于配置的`.02f`，使合法周期在第一步触发`SP_FAULT_CONTROL`。旧合成板测只有10ms周期，未覆盖该边界。

本轮改为一次`(float)dt / 1000.0f`，检查与规划使用同一个结果。未扩大周期上限、未放松采样身份/时限检查。新增完整监督/规划路径的1、10、19、20ms接受及21ms拒绝测试；新测试在修复前失败、修复后通过。板端纯计算新增20ms正常跟踪场景，覆盖目标编译器的实际浮点行为。

## 原始750帧的实际输入覆盖

逐帧验证原始VQF的CRC、SHA和元数据，调用当前原生视觉算法、米制适配器及规划器。分类与实际适配器接受结果在750帧全部一致。

| 结果/首次阻断原因 | 近预览 | 完整预览 |
| --- | ---: | ---: |
| 接受 | 36 | 3 |
| 固定采样行缺少两条实测边 | 400 | 496 |
| 其他几何/历史检查 | 1 | 6 |
| 超出当前配置地面适用域 | 313 | 245 |

这是输入接受覆盖，不是识别正确率。当前完整预览仅第560、561、563帧可用，不能称真实道路输入已打通。

为隔离问题，规划组合明确使用静止合成编码器、20ms均匀相对时间、软件调用者准入声明；没有伪称同步运动实测。修复前第560帧准入后，第561帧立即故障锁存；修复后产生15次READY，后续几何缺失导致近场抑制/恢复、受控停止，最终STOP_LATCHED，无自动重新准入。地面矩阵、适用域、动态参考参数均未更改。

还比较了一个仅离线的采样实验：保持近/完整两端行和地面域，只在中间实测双边行重新均匀采样。近预览由36增至66，完整预览仍为3，其他几何/历史拒绝增加。因此未将它纳入固件；它不能解决当前完整输入阻断。

## 软件、构建与板端结果

| 项目 | 结果与范围 |
| --- | --- |
| 修复前复现 | `20261109T031557-2f6a3b1378`，planner-shadow新增20ms边界断言FAIL，故障证据保留 |
| 完整GCC optimized Host | `20261109T031732-337ebf44f9`，58 PASS / 1 SKIPPED；performance-host仍缺数据集/批准基线 |
| 静态与工程结构选择 | `20261109T031732-7bafb9558f`，environment/static-safety/project-structure共3 PASS，非完整PreFlash |
| 原生监督专项 | 159条检查通过；V3候选专项3525条检查及原PI/PWM序列对照通过 |
| Keil完整构建 | 0错误、0警告；Code104848、RO15536、RW212、ZI211116字节 |
| V3下载 | 首次RDDI-DAP失败保留；操作者恢复连接后11:20:33 Programming Done / Verify OK |
| 运行身份 | 操作者RST后11:21:51经COM10核对`PHASE3_C26_SHADOW_20261109_3`，配置`C26-SHADOW-FA161BDE` |

V3停机计算每次4场景、256tick：原10ms正常跟踪/近场恢复/远场受控停止，新增20ms正常跟踪。每次238 READY、18 ZERO、250条件检查、238过期拒绝，三次均PASS。测试局部状态不进入实际PI/输出所有者；前后PWM0/0、faults0、编码器累计0/0不变。

| 次序 | 平均周期 | 最大周期 | 按600MHz换算平均 | 换算最大 |
| --- | ---: | ---: | ---: | ---: |
| 1 | 22116 | 42728 | 36.860µs | 71.213µs |
| 2 | 22052 | 35017 | 36.753µs | 58.362µs |
| 3 | 22134 | 38254 | 36.890µs | 63.757µs |

仅为5点合成预览下`vps_tick`的DWT计时，包含监督/规划，不含视觉、适配、PI、最终writer或串口；不是全链路最坏时延。ACTIVE=0、PROFILE_REVIEWED=0，原PI增益与PWM上限未改。

## 动力隔离的静止ON：部分验证，未通过完整共存验收

操作者确认电机/舵机动力断开，C27 OFF，C26短暂ON后回OFF。11:23:43导出冻结影子环115条记录，跨度1133ms，run=1、overwritten=0；它证明实际RUNNING/输出租约窗口调用了前台诊断，不代表物理动力或米制规划准入。

当前画面near_geometry/full_geometry均0，原视觉仍报告TRACK/valid=1。影子session=0、plans=0，115条记录均无有效米制结果。因此真实米制输入仍是主要软件阻断，不能把该ON窗口写成规划器/完整共存PASS。

原速度环随后锁存`VC_FAULT_ENCODER=256`，具体`no_count_left`，保留诊断为control_age=9ms、sample_age=0ms、interval=10ms。`LAST_STOP`为run_ms=1134，原目标R/L=120/123、原PWM命令840/850（千分比）。这证明原控制曾计算非零目标/命令，动力隔离条件下不等于实际施加转矩、带载PI稳定或物理停车；无计数分支也不能当作调度超时。故障后读回目标/PWM均0。影子诊断中的零目标与原控制的这些目标属于不同来源。

冻结数据和故障保留，未清除、未重置保护。下一步先处理真实图像的采样行与米制适用域匹配，利用现有图像验证近/完整预览的有效距离和连续性；之后再进行可信输入下的ON会话与实际反馈联调。当前不具备接通动力后的米制闭环验收条件。

## 当前图像模块诊断：已发现局部误分割

11:29:28开始在OFF状态抓取当前灰度、原生叠加图，各2帧；随后补抓原生二值图2帧。灰度图可见白色赛道旁的亮灰地面，二值图将其中一片地面也归入白色，叠加图的中远场左边界随之进入背景，拉偏中线。因此已经取得局部二值化/边界误跟踪的直接图像证据，不能继续仅以TRACK/valid说明识别可靠。

例如原始行54对应预览行27，两个叠加图均出现灰边界标记x=24/71、中线黑标记x=46/47。按绘制规则，中线整数原始像素在94～95间；用现有矩阵计算其横向坐标约为−133～−125mm，超过配置的−100mm下限。该行是近预览的末端。适配器实际使用未取整的双边中点及空间中值，此处不伪称直接读取了精确原生浮点坐标；量化及1像素差异不改变此处偏出适用域的判断。

结论：图像前端的局部误分割是当前问题的明确组成部分，米制域检查进一步阻断该输出；地面矩阵本身仍有独立误差，不能据此宣布标定完全正确。左轮无计数属于动力隔离测试的另一条故障链，不能归因于图像。下一步优先检查按行阈值将亮灰地面并入赛道的情形，再回归边界连续性和米制输入覆盖；当前未扩大地面域或修改识别门限。

证据：[灰度图](evidence/vision-current/g-0.gray.png)、[原生二值图](evidence/vision-current/b-0.gray.png)、[原生叠加图](evidence/vision-current/o-0.gray.png)、[投影核对](evidence/vision-current/analysis.json)。预览为94×60抽样，显示图仅作最近邻放大；三种模式顺序采集、不是同一曝光；协议无像素CRC和帧号，现场观察也未形成完整人工真值数据集。判断范围限于可见局部错误，不提供全图识别正确率。

## 工程与复现

下载入口：[MPC_Phase3_Shadow_Download.uvprojx](../../../.build/phase3-real-input-20261109/delivery/workspace/demo/mdk/competition/MPC_Phase3_Shadow_Download.uvprojx)。同目录`MPC_Phase3_Shadow.uvprojx`只构建；原工程默认配置不被生成器修改。产物、SHA、下载和运行证据见[evidence.json](evidence.json)。

```powershell
python -m tools.speed_planning.audit_metric_input --output .build/phase3-input-audit-new
python -m tools.speed_planning.audit_metric_input --output .build/phase3-input-resample-new --paired-resample
python tools/testing/run_tests.py --profile Host --suite planner-shadow --suite phase3-shadow-preparation --compiler gcc --jobs 2
python tools/prepare_phase3_shadow.py --output .build/phase3-v3-rebuild-new --build
```

以上命令不烧录或驱动车辆。输出目录必须不存在。

来源层：原生C、VQF字节、JSON、Keil XML和串口字段为`structured_parsed`；CRC/SHA、Host断言、计数和DWT换算为`computed`。物理摆放/动力隔离按操作者确认记录。没有仅`md_extracted`关键数值；没有新增物理映射、制动能力、动态时延或转角曲率认证。未修改全局配置、技能或持久Agent行为。
