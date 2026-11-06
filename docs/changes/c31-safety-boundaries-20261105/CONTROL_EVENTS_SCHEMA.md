# Phase 1：控制事件与固件身份方案

状态：待确认设计，未实现。保持 SCRAW-1 的 FRM1/GAP1/END1 和像素负载不变，在 session 目录增加 `control_events.jsonl`。

## 队列与所有权

Control 为事件唯一生产者，Storage 为唯一消费者及唯一 FatFs 写入者。ISR 只锁存撤销原因，Control 收集后发布；Link/Storage 通过固定请求状态与 Control 握手，不直接向事件队列写入。

建议静态队列首版容量 32 项，每项固定 C 结构不超过 96 bytes，预算不超过 3 KiB；这是内存/吞吐工程选择，不是安全时限标定。实施前以 sizeof、MAP 剩余内存和现有队列实现复核。Control 只零等待入队，不执行 JSON 格式化、文件写入或动态分配。

事件有独立单调序号，即使入队失败也消耗事件序号并递增丢失计数。保留一个固定终止快照槽，承载最新终止状态、原因累计位图及版本；它不保证保存无限次中间事件。消费者合并 FIFO 与终止槽时按事件序号去重/排序。队列丢失、序号空洞或终止槽合并均使事件流标记不完整，不能宣称有限内存能抵抗无限存储停滞而无损。

帧记录阻塞或 SD 错误不能反向影响 Control；sidecar 写入失败遵守现有 Storage 隔离/错误收尾策略，不能新增第二写入者。是否继续帧记录须按可维持的文件状态处理并明确标记事件不完整；不得阻塞控制来挽救日志。

## 事件字段

| 字段 | 语义 |
|---|---|
| schema_version | `control-events-1`，独立于 SCRAW 的版本 |
| session_id / boot_instance | 会话和 boot 关联；boot 标识仅用于关联，不宣称密码学唯一或设备镜像认证 |
| event_sequence | 本 boot 的事件序号；明确位宽，JSON 不经浮点转换丢精度 |
| timestamp_us | 同源 GPT1 时间扩展，单位 us、实际分辨率 1000 us；不是曝光时刻 |
| event / previous_state / state | 初始快照、启动、失效、故障、STOP、终止与关闭快照 |
| trigger_frame_sequence / trigger_capture_us | 视觉触发时对应实际已消费观测；非视觉触发为 null |
| latest_control_frame_sequence | 最近控制消费帧，区别于最近 CSI 完成帧；未消费时 null |
| faults / stop_reasons / vision_reason | 累积故障位图、停止原因与当前视觉原因 |
| near_valid / far_valid / recovering | 接受事件时的证据，不将旧帧标志冒充当前帧 |
| target_right / target_left | 轮速目标及其单位 ticks/10ms；不可得时 null |
| pwm_right / pwm_left | 软件已提交 PWM 的 permille 快照；不是实测电压/转速 |
| duty_target / applied_cap_permille | 目标 duty 与最终输出限制分开记录；无可读值为 null |

示意记录（不是运行采集数据）：

```json
{"schema_version":"control-events-1","session_id":"example","boot_instance":"example-boot","event_sequence":7,"timestamp_us":1600000,"event":"VISION_DEGRADED_FAR","previous_state":"NORMAL_TRACK","state":"DEGRADED_FAR_LOSS","trigger_frame_sequence":80,"trigger_capture_us":1600000,"latest_control_frame_sequence":80,"faults":0,"stop_reasons":[],"vision_reason":"FAR_LOSS","near_valid":true,"far_valid":false,"recovering":false,"target_right":0,"target_left":0,"pwm_right":0,"pwm_left":0,"duty_target":0,"applied_cap_permille":0}
```

## 开始与关闭握手

Storage 创建 session 标识并请求 Control 开启事件覆盖。Control 在自己上下文写初始状态快照并发布起始水位；Storage 确认后开始记录窗口。若车辆先于开录开始运行，初始快照仍能解释第一批记录帧。帧与事件按真实时间关联，不宣称逐帧锁步。

普通 `@rec stop`：Capture 停止接收新记录帧并 ack；Control 写关闭边界快照、封闭该 session 的事件入口并发布末水位；Storage 排空已接受帧/事件、sync/close，再写 manifest 并卸载。截止后的事件属于覆盖区间外，不能因此停止车辆。

`@export`：先由 Control 撤销运行授权、提交双轮零输出并发布终止快照/输出抑制 ack，Storage 才进入记录停止与上述排空流程。不能只依赖 Control task priority 高于 Storage。若 ack 缺失或超时，Storage 按已有有界故障机制结束等待，safe_to_remove 不得为真，Control 始终不等待 Storage。

同一 Control poll 中先处理 STOP/FAULT，再处理关闭请求；末水位包含该次终止事件。会话截止后发生的 STOP 由覆盖区间外语义处理。状态快照跨任务发布要按 epoch/固定整数副本建立一致性，不用 volatile 代替完整协议。

关闭完整性至少包含：文件名、覆盖起止时间、首末事件序号、入队/写入/丢失/未持久化数量、终止快照状态、完整/不完整/未知及原因。只有末水位排空、同步和关闭成功才能写 complete。断电、末行截断、缺失 manifest、SD 写失败或不可取得的丢失计数都不能被默认为 complete。

## 时间、解析器与二进制身份

当前 CSI IRQ 用 GPT1 毫秒扩展成 64 位后乘 1000。控制事件复用同源时间；64 位读写在 32 位 core 上必须有短快照保护。合法回绕、序号截断和启动初始时间分别测试，不用 Link 到达时间代替采集时间。

PC importer 保留原始 sidecar 字节到 A0 并哈希；独立解析到派生控制事件索引，校验 session、序号、时间、必需字段与截断。旧 session 没有 sidecar 时仍能导入图像，但控制事件完整性应为 unavailable，不能写 complete。独立 schema 和 manifest 字段允许旧 SCRAW payload/parser 继续工作；实施时检查已有 session schema 是否允许扩展字段。

固件携带链接前生成的 build_id、Git/dirty、输入树与关键配置。链接后外部 build manifest 记录同一 build_id 对应的 AXF/BIN/MAP SHA-256。PC 仅在身份一致且匹配唯一时关联 BIN hash；缺失/歧义保留 null 与原因。输入树 hash 不冒充二进制 hash，禁止将最终二进制全文件 hash 嵌回同一二进制。

build_id 要每个构建实例可区分，并通过生成契约测试定义其与增量构建的关系；不能在生成器无变化调用时悄悄刷新 ID、破坏现有幂等契约。建议在显式构建调用边界生成、传入 prepare；生成器相同输入仍保持字节稳定。只建立构建产物与会话声明身份的关联，不宣称设备上实际烧录镜像已验证。
