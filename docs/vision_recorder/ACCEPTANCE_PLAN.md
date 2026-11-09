> **维护口径（2026-11-09）：** Phase2实测与诊断工具已单独发布于bd94f27；没有据此验收Recorder运动写卡、Camera/SD/PI组合或正式训练数据集。现有静止/软件结果按本文原构型阅读；其他未提交Recorder/Phase3实现不能写成main已发布或当前车载配置。 统一状态与历史适用范围见[文档维护总览](../DOCUMENTATION_STATUS.md)。

# 持续边采边写：实施和验收计划

原计划日期：2026-11-04；维护：2026-11-07。Recorder/RTOS、SCRAW-1 PC adapter、独立测试与构建已实施，源码现已合入main，见[实施记录](IMPLEMENTATION.md)。后续B2独立静止短窗、B3零实际输出记录和独立SD实测已有局部证据；H0组合边界尚未全部关闭，H1/H2/H3也未完成。PC/Mock或局部静止通过不算持续运动采集与稳定行驶通过。[当前工程导航](../PROJECT_INDEX.md)

## 1. 验收原则

用户明确的最终效果：**车辆稳定行驶，同时持续采集和写 SD；速度当前不设成绩要求，
必要时可牺牲，但稳定性必须保证。** 只作为独立采集训练构型，不作为正式赛道版本。

正常合格配置的全段目标为零记录丢帧、无损像素、源序号和真实结束可追溯；异常时
记录功能停止并如实判 incomplete，而车辆不因记录失败失去控制服务。该异常容错
不能被包装成“故障卡条件也完成了连续全帧采集”。

## 2. 按阶段推进

| 阶段 | 工作及交付 | 完成门槛 |
| --- | --- | --- |
| D：设计/授权，已完成 | 架构路线、格式提案、预算、未决事项和验收计划 | 用户已批准仅采集训练分支引入最小 RTOS |
| S0：隔离与依赖 | 独立分支/工程、静态内存、内核/端口版本/许可、IRQ/timebase/OSA 映射 | 原 main 生产入口可保持，完整源身份和原工作区被保存 |
| S1：可移植 Recorder 核心 | 池/队列状态机、独立命令、统计、分段、格式编码、mock storage | 所有权/队列/失败隔离自动测试通过，不依赖板端 |
| S2：板级适配 | Capture source/handoff、OS 等待、单 Owner FatFs、finite timeout/abort | 相应代码审查、共享状态模型、ARM 编译和专用工程链接 |
| S3：正式 PC adapter | 正式导出、CRC/恢复、Session/Frame schema 及 PNG | 独立 C producer → Python parser golden bytes 与故障样本通过 |
| H0：授权后的静态板台架 | 上电不运行电机，Capture/SD/时钟/引脚/Cache/内存可达性 | 无缓冲复用、真实像素、卡异常/无卡可退出；不得直接从纯构建进入地面运行 |
| H1：授权后的架空闭环 | CPU/总线/IRQ 压力及编码器、PI/PWM、任务时序 | 记录开关和长尾注入不导致样本积压、期限破坏或输出复活 |
| H2：授权后的低速赛道 | 固定配置进行记录关/开对照，异常介质分开测 | 稳定循迹、正常连续记录和最终 PC 完整性；不得只展示静止采集结果 |
| H3：长时与泛化 | 多张卡、温度/电量/光照/赛道、长段/拔卡/断电 | 完整统计、故障边界、最终认可的采集配置与安全收尾流程 |

每个阶段都有真实执行身份/输入和产物哈希，失败项保留。实现和硬件验证授权分别
处理；当前授权已覆盖软件实施，仍不在此文档里自授烧录或真实卡操作权限。

## 3. 独立软件测试矩阵

| 领域 | 最少覆盖 |
| --- | --- |
| Capture 所有权 | 活动 DMA 地址不与 CPU/写入持有地址重叠；generation 防 ABA；替换再发布；两 done/IRQ 延迟歧义；饥饿/队列满不回收视觉持有帧 |
| 快照 | 输入在整个复制期间只读且有效；大小/stride/序号/时间一致；记录失败不改变视觉消费者 |
| 队列/池 | producer/consumer 交错、满/空、counter wrap、停止边界、重复释放/非法状态拒绝、从不等待空槽 |
| 状态机 | 未初始化/no card/重复 start/重复 stop/stop during prepare/排空超时/错误后不自动重启 |
| 存储接口 | init/mount/create/preallocate/write 短写/满卡/sync/close 失败；卡移除；命令无 completion；stale IRQ；abort 后 DMA 尚忙的 buffer 隔离 |
| 超时 | 绝对 deadline，重试不延寿；timer wrap；OS tick 与 GPT 单位；无卡不进入无限插卡等待 |
| 命令隔离 | Recorder start/stop/status 不调用驾驶 arm/clear/stop，不改 C26/C12 模式或 PWM；驾驶 STOP 不可被记录状态撤销 |
| RTOS 契约 | 所有 drive 调用在 Control；Vision result 原子发布；高优先 ISR 不调用越界 OS API；Storage 不持有 Control mutex |
| 格式 | 独立 golden bytes/CRC 向量/无 padding/unknown version/标志/metadata/记录长度/位翻转/像素内 magic/部分写 |
| 恢复 | 分段缺失、END 缺失/截断、JSON 缺失、预分配旧尾/零尾、乱序/重复帧、未知丢失原因、异常时间、回绕 |
| PC | C 生产者输出 → Python 校验、原字节不变、PNG 每像素相等、导出 incomplete 不覆盖源、旧 draft 明确隔离 |

Mock 测试只能证明约定的软件状态，不证明 MCU DMA/Cache、物理写入和实际最大延迟。
软件核心测试独立执行，不以改全局 C32 Runner 为前提；后续可受控注册消费者。

## 4. 纯构建和静态审查

- 专用 recorder_train 工程完整链接，检查新 map、栈/静态对象、non-cache section、
  原 profile Flash 区不受覆盖；原 switch/auto-track 保留构建身份。
- 固定 OS 端口、FPU/异常上下文、SysTick/SVC/PendSV 符号唯一性；删除采集构型中
  bare-metal polling OSA 冲突，不能同时链接两个实现。
- IRQ 优先级与 OS API 边界表，所有 PRIMASK 长段和 Cache 操作范围审查；SDK
  mutex/event/卡忙/插卡/重试/恢复所有层次都具备可执行的有限预算。
- 所有 FatFs 调用点由 Storage owner 收敛；不在 CSI/PIT/Control/Vision 中调用。
- Recorder 和存储不得调用 `pwm_set_duty` 或申请驾驶授权；原安全撤销路径仍可独立执行。
- LFN、f_expand、file limit、真实 sync/abort 的构型要与代码一致，不能只修改文档。
- 原生产控制/视觉契约和受影响构型测试必须通过，不能以降速为由放宽安全断言。

## 5. 硬件指标与稳定性判定

### 需要记录的时间线

Capture IRQ 事件/歧义、源序号与采样时间、复制开始结束、视觉完成、Control
取得结果、编码器采样/消费、SD submit/completion/同步/abort、队列水位和错误。
用于验证的时序可以通过独立台架记录/引脚测量等获取，具体通道需另行批准；首版
原始图像格式不扩展为全量遥测系统。

### 硬门槛

1. 保持现有采样/新鲜度/控制服务期限，不能靠放宽 10/20/30/50/100ms 保护通过。
2. 无持有帧被 DMA 或其他任务覆盖，无不明混合帧被当作可信数据。
3. 没卡/失败卡/满卡/异常 completion 不阻塞 Control，存储退出不会引起误 arm、
   重新启动或释放仍在 DMA 使用的内存。
4. 记录开启的正常行驶不新增异常停车、卡顿、振荡、出界或路线漏段；原本未通过的
   环岛/十字能力保持“未完成”，不能凭采集项目宣称已修复。
5. 正常合格卡/配置的一次完整训练 Session，源头可观察范围内无未解释丢失，全部
   接收记录完整落盘；PC CRC/序号/时间/结束/像素核验一致。
6. 异常断电可能只恢复完整前缀，需如实报告丢失窗口；不能承诺消费级卡绝对掉电持久。

速度暂不设最低或最快目标。先使用稳定的低速训练 profile，记录确切参数；降低
车速后仍须满足上述门槛，不把极慢/停车采集冒充正常连续行驶。

### 对照方案（建议，实际阈值/次数审核后固定）

相同训练固件、安装、赛道和电池条件下做 Recorder OFF/ON 的配对试验，包含直线、
连续弯、十字和入口视角。先架空，再低速；至少记录成功/尝试数、每次故障/出界、
局部/整体时序和数据完整性，不能只报平均 FPS 或成功视频。正常对照和故障注入
分开，异常只证明容错，不算正常全帧录制通过。

## 6. 取卡导出验收

独立请求 record_stop，采集入口在 cutoff_seq 后拒收，后台排空/sync/close；确认
`safe_to_remove` 后停车取卡。若车辆尚未停稳，即使数据已 close 也不能把提示解释
为可以接近旋转机构。取卡复制由独立授权执行，PC 工具只读源文件、不格式化。

正常 closed 与 incomplete 两类样本都应导出；二进制 A0/metadata 与 SD 源哈希相同，
每帧 PNG 与原 payload 的可见像素一致。缺 session.json 的掉电会话不能直接被旧
draft importer 拒绝后丢弃，应由正式 recovery adapter 给出可追溯恢复报告。

## 7. 每轮验收记录

产出 SOFTWARE_VALIDATION.md（Host/mock/静态/纯构建分别列）、硬件授权后另写
HARDWARE_VALIDATION.md；数据和日志在独立目录。实际实现哈希、内核/端口、
卡片信息、测量方法、失败与未测项必须记录。当前只有设计，以上均不列为 PASS。
