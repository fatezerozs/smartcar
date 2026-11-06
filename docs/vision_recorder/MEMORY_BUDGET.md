# 持续采集的内存、吞吐与时序预算

日期：2026-11-04。状态：**含首版链接 map 检查；时序、吞吐和 DMA 可达性未实测**。
目标仅用于独立采集训练构型，正常行驶并持续写 SD，稳定优先、车速可调整。
总体设计见 [ARCHITECTURE](ARCHITECTURE.md)。

实施后的 `computed` map：non-cache 使用 1664448B / 2097152B，余 432704B；
8×22592B raw、64×22656B recorder、32KiB bounce、卡内部对象和描述符均在该区。
没有采用下文设计候选的双 64KiB staging。静态任务栈 53248B + idle 1024B；
布局自动校验见 `tools/prepare_recorder.py --check-map`，实际构建身份与地址见
`.build/recorder-train/build-evidence.json`。这不是 DMA/Cache/堆栈深度的硬件证明。

## 1. 已知配置和区域

源码配置：188×120、GRAY8、50FPS（`zf_device_mt9v03x.h:77–85`）。真实 Sensor/
Capture FPS 尚未测量。每帧原始可见像素 22560B，不主动缩放或抽帧。

链接脚本 `demo/mdk/scf/MIMXRT1064xxxxx_flexspi_nor.scf` 定义：

| 区域 | 基址/长度 | 当前可下的结论 |
| --- | --- | --- |
| DTCM | `0x20000000` / `0x70000`（448KiB） | 现有数据、堆栈；不能将新图像池直接塞进普通全局 ZI |
| OCRAM | `0x20200000` / `0x80000`（512KiB） | 有单独 section，但 DMA 路径/Cache 策略仍需查证 |
| SDRAM 代码/Cache | `0x80000400` 起，约 30MiB 区域 | 应用代码在此执行；不是整块可随意分配的空闲堆 |
| SDRAM non-cache | `0x81E00000` / `0x200000`（2MiB） | 首选候选图像池/SD staging 区，待新构型 map 与 DMA 可达性验证 |

MPU 证据：`libraries/sdk/board/board.c:349–359` 对 SDRAM 和最后 2MiB 分区；
`libraries/zf_common/zf_common_clock.c:78–83` 在时钟初始化调用 `BOARD_ConfigMPU`。
section 命名见 `zf_common_typedef.h:171–183`。这些代码说明配置意图，不证明新增
CSI/USDHC DMA 数据路径已经正确，更不代替缓存/总线/链接实测。

历史 `.build/midterm-auto-track/Listings/rt1064.map` 修改时间为 **2026-10-27**：
`RW_m_data Size=0x2C314`、`SDRAM_ncache Size=0`、Total RW=191252B。
它早于本次构型，**只能作参考，不能作为新增任务、栈或池的剩余空间证明**。

## 2. 数据速率和候选内存

按提议正式协议每条 FRM1 额外 44B，连续流不含 metadata/GAP/END 时：

`22560 × 50 = 1,128,000 B/s` 原始像素；
`(22560 + 44) × 50 = 1,130,200 B/s` 文件流量。

候选布局取 64B 对齐，payload 向上对齐到 22592B，每 Recorder 槽再预算 64B
descriptor/状态空间；最终以 `sizeof`/section/对齐断言复核，不把 C struct 默认
填充当作磁盘协议。

| 项目 | 候选规模 | 字节 |
| --- | ---: | ---: |
| Capture Pool | 8 × 22592 | 180736 |
| Recorder Pool | 64 × (22592 + 64) | 1449984 |
| SD writer staging | 2 × 65536 | 131072 |
| 对齐/索引/SD 辅助区预留 | 65536 | 65536 |
| 合计 | — | **1827328** |
| 相对 2MiB 的账面剩余 | 2097152 − 1827328 | **269824** |

Recorder/Capture 对象不一定都需 non-cache；首选放上述区域降低跨 Cache 维护的
复杂度。8 个 Capture 槽只是模型起点，2 个活动槽、紧急替换槽和消费者持有数要
纳入不变量；不可因配置为 8 就断言不会饥饿。64B 是分配对齐建议，不是声称本芯片
所有 DMA 的实际 Cache line 均为 64B。

任务栈、RTOS 控制对象和既有控制状态另计；初始设计预留可为：Control 8KiB、
Capture 4KiB、Vision 16KiB、Storage 12KiB、Link 8KiB、Idle/Timer/MSP 及内核
额外至少 16KiB。**共 64KiB 只是起始预算**，需栈水位、最坏调用链/FPU/中断嵌套
复核。避免把此 64KiB 与上表 SD 辅助区当作同一块。全部静态分配，采集时无堆扩张。

## 3. 长尾缓冲计算

设 `F` 为真实 Capture FPS，`Tstall` 为没有任何记录槽释放的最长区间，`Q0` 为
开始时已有占用，`Nreserve` 为安全余量：

`Nrequired ≥ ceil(F × Tstall) + Q0 + Nreserve`。

以 F=50、Q0=8、Nreserve=8 的**计算场景**：

| 无释放区间 | 期间到帧 | 总需槽数 | 64 槽能否满足该假设 |
| --- | ---: | ---: | --- |
| 100ms | 5 | 21 | 账面可满足 |
| 250ms | 13 | 29 | 账面可满足 |
| 600ms | 30 | 46 | 账面可满足 |
| 1000ms | 50 | 66 | 不满足 |
| 1500ms | 75 | 91 | 不满足 |

64 空槽理想填满时间为 1.28s，但已有 backlog、预留槽、在途 staging、sync、
调度剥夺等都会减少它；**不能把 1.28s 直接当作已证明的可接受卡停顿**。
上述计算随本轮文档保存在 `.build/vision-recorder-design/budget.json`，不是实测。

设实际文件流到达率 R，卡恢复后的有效排空速率 W：须 W > R，backlog 排空的
近似下界为 `backlog_bytes / (W − R)`。例如假设 W=3,000,000B/s，600ms 的
新到数据约 678120B，则理想净排空约 0.363s；该例不含额外 metadata/同步和 CPU
预算，也不是卡速承诺。平均写速够快而连续长尾/无空隙仍会溢出，需整个忙周期模型。

高水位仅报警/拒绝新的记录，不强制停车、不主动改 Capture FPS、不覆盖旧未写帧。
有限池不能容忍无限介质停顿；合格卡和合格环境以整段零丢帧验收，不合格则停止记录、
车辆控制继续独立运行，本次数据判不完整。

## 4. 时序约束

| 路径 | 当前约束/候选要求 | 尚需证据 |
| --- | --- | --- |
| 编码器安全采样 | 保持名义 10ms、最大 20ms | PIT 实际间隔、最长关中断时间 |
| 新样本消费 | 保持未消费上限 1、年龄 30ms、控制停顿 50ms | Control 任务唤醒及样本到 PI 的响应时间 |
| 视觉帧有效期 | 保持 100ms 保护 | Capture/Copy/Vision/Result/Control 全链帧年龄 |
| ISR | 只处理 descriptor、寄存器/计数 | CSI/PIT/USDHC ISR WCET、优先级和系统屏蔽 |
| 复制与 CRC | 快照在任务，CRC 在 Storage | 22560B 复制、CRC、Cache clean/invalidate 和总线竞争 |
| 文件操作 | 只阻塞 Storage，有限总 deadline | mount/create/expand/write/sync/close/abort 最大时长 |
| CPU 分配 | 高优先任务须真实阻塞、存储须有排空预算 | 最坏忙周期，而非“平均 CPU 还有空闲” |

固定优先级响应分析至少纳入 `Ri = Ci + Bi + Σ ceil(Ri/Tj)×Cj` 的任务干扰，
以及实际 IRQ、总线/Cache 和共享资源阻塞；该式是规划模型，不能从未知 Ci/Bi
得出调度通过。实现已采用 Control 每 1 tick 让出，但实际周期/抖动仍须测量。

允许选更低的训练车速，以增加横向稳定性余量；现有 PI/安全期限不能因此放宽。
若原视觉计算无法和记录并行满足预算，先定位并减少非关键诊断/模型计算，或审查
算法资源使用；不得降低记录目标为只存处理帧来假装全帧采集。

## 5. 实施后的定容门禁

必须产出新构型 map/区域使用、静态尺寸与对齐断言、DMA 缓冲可达性/Cache 协议、
堆栈水位、Capture/Storage 长尾和最坏调度证据。任何一项未知都保留 OPEN，不用
旧 map 和 Python 算式替代。正常速度可降低，稳定性及源帧真实性仍是硬门槛。
