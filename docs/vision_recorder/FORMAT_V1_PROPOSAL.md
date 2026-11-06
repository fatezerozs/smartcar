# SCRAW v1 正式格式提案

日期：2026-11-04。状态：**训练版已实现 C 生产者和 Python adapter，板端验收/最终冻结待完成**。
实际差异见[实施说明](IMPLEMENTATION.md)：目录为 `0:/R0000001/`；最终 `session.json`
仅成功收尾后创建；flags 位 0 表示无对应源帧，FRM1=0、GAP1/END1=1，其他位拒绝。
未启用预分配；GPT1 毫秒值转换 us，分辨率为 1000us。不能称作已实车验证的格式。
这是独立采集训练构型的候选协议，与现有 [SCRAW-DRAFT-1](DATA_FORMAT.md) 通过
不同 magic/version 区分。不得静默将旧 synthetic 文件改成正式数据。

目标：连续原始像素、独立源帧身份、损坏/丢帧可追溯、分段和掉电后完整记录恢复。
不采集全量车辆遥测；稳定性测量可使用独立台架日志，不塞进首版图像 payload。

## 1. 会话与目录

候选目录：`/REC/<session-token>/session.json` 和 `000000.scraw`、`000001.scraw`…。
必须采集构型单独启用/测试 LFN（建议 FF_USE_LFN=2），否则改为 8.3 名字并另定
映射；当前默认 LFN=0 不支持这里的文件名。

Session ID 的权威为 128bit session_uuid；目录 token 是短定位名，不能替代 UUID。
生成源需核验板端唯一身份和独立 session counter/随机源，并用排他创建检查碰撞。
不得假定 RTC 有效，不通过改 Flash 车辆配置区存任意采集 ID。首版组合 OCOTP
CFG0/CFG1 影子寄存器、卡 CID 序列号与排他目录计数，不是 RFC 随机 UUID；同卡
格式化后可能复用，PC 拒绝已有身份覆盖，见 [OPEN_ISSUES](OPEN_ISSUES.md)。

`session.json` 包括：format/schema_version、UUID、采集版 build 标识、完整 firmware
SHA/dirty 或无法确认的 null、camera 配置、width/height/stride、时钟单位/精度/事件、
configured_fps、实际可测 Capture 频率、采集窗口、分段名单、结束/错误和三种帧计数。
未知量用 null；不能把 firmware SHA 的编译字符串当作自动证明源树干净。

初始 metadata 写在准备阶段，文件记录逐条持续落盘；最终 sidecar 只在收尾更新。
PC 不能依赖最终 sidecar 存在：每段二进制头都带会话身份与必要几何信息。掉电时
PC 可只读发现分段并生成 **recovered/incomplete** manifest；不回写源卡修复文件。

## 2. 段头（固定 64B + UTF-8 metadata）

所有整数小端，无隐式结构体 padding。保留字段必须 0；未知 major 拒绝，minor
的兼容政策在实现前固定。metadata 最大 4096B，Header CRC 先校验再相信长度。

| offset | 字段 | 长度/约束 |
| ---: | --- | --- |
| 0 | magic | 8B：ASCII `SCRAW1\r\n` |
| 8 | major | u16 = 1 |
| 10 | minor | u16 = 0（提案） |
| 12 | header_size | u16 = 64 |
| 14 | flags | u16；当前 0 |
| 16 | segment_index | u32，0 起连续 |
| 20 | width | u16；实际 Capture 宽 |
| 22 | height | u16 |
| 24 | stride | u16，≥width |
| 26 | pixel_format | u16：1=GRAY8 |
| 28 | timestamp_unit | u8：1=us |
| 29 | timestamp_event | u8：1=CSI IRQ 采样；2=完成回调采样；均不是曝光 |
| 30 | reserved | u16=0 |
| 32 | session_uuid | 16B，不解释为字符串内存布局 |
| 48 | metadata_length | u32，1..4096 |
| 52 | metadata_crc32 | u32，覆盖紧随头部的原始 UTF-8 JSON 字节 |
| 56 | reserved | u32=0 |
| 60 | header_crc32 | u32，覆盖 offset 0..59 共 60B |

各段固定字段/metadata 共享身份和不变配置，segment_index 可变。相机设置/时钟
语义变化必须新 Session，不能在同一会话中静默换几何或改变时间单位。

## 3. 记录头（固定 40B）和后缀

| offset | 字段 | 长度/约束 |
| ---: | --- | --- |
| 0 | tag | 4B：`FRM1` / `GAP1` / `END1` |
| 4 | record_size | u32 = 40 + payload_length + 4 |
| 8 | source_sequence | u64；FRM1 为可信完成事件 ID；其他记录含义见下 |
| 16 | timestamp_us | u64 单调时钟；分辨率不能由单位推断 |
| 24 | payload_length | u32，FRM1 必须 stride×height |
| 28 | payload_crc32 | u32，仅覆盖 payload 原字节 |
| 32 | flags | u32；时间质量/歧义等位含义需冻结；未知位不忽略 |
| 36 | record_header_crc32 | u32，覆盖 offset 0..35 共 36B |
| 40 | payload | 可变 |
| 40+payload_length | record_crc32 | u32，覆盖整个 40B header + payload，不包括本后缀 |

FRM1 单帧开销 44B，188×120 图像时记录 22604B。不按扇区给单条记录填充，
Storage 将字节流聚合成多扇区请求，记录可跨 staging 边界；不得依赖 `sizeof(C struct)`
序列化。Raw payload 包括 stride padding，不缩放、裁剪、过滤、叠字或有损压缩。

CRC32 采用 reflected IEEE：多项式 0xEDB88320、初寄存器 0xFFFFFFFF、final xor
0xFFFFFFFF；与 zlib `crc32(bytes, 0)` 等价，标准向量 `123456789 → 0xCBF43926`。
头、payload、完整记录各自独立重置计算。CRC 检测损坏，不是签名或内容认证。

## 4. 序号、时间和三种帧率

源头在可信 Capture 完成事件分配 source_sequence，分段/复制/排队不得重新编号。
如只能数“收到的回调”，必须写 sequence_semantics=`observed_completion_events`，
并统计双 done/未知丢失；不能将其改名 sensor_frame_id。无法计数的源头损失用 null。

timestamp 为 IRQ 或回调实际采样的独立单调时钟，不是任务处理/SD 写入时刻，也不
是 PC 接收时刻。优先核验微秒 GPT2 与溢出扩展，GPT1 毫秒控制时钟保持原语义。
32bit 回绕扩展、ISR 延迟、缓存和异常恢复需要独立板端证据；若当前只能可靠取
毫秒值，可换算成 us 数值但必须标明 resolution_us=1000，不能宣称微秒精度。

单独保存 configured_sensor_fps、measured_capture_fps、vision_process_fps、
record_write_fps；记录接收数、写入数和已同步边界亦分开。Recorder 在途数量不能
算成功落盘，f_sync 返回不证明卡掉电后绝对持久。

## 5. GAP1 / END1

GAP1 payload 为 ≤4096B JSON：原因（pool_full、queue_full、capture_ambiguous、
storage_error 等）、已知序号区间/数量、unknown_loss、发生时间区间。不能确定数量
时字段 null；源帧未产生与系统未观测不是同一原因。GAP1 自身无可信 source 帧时
头部 source_sequence=0 并用专门 flag 表示，而非与 FRM1 的 0 混淆。

缺记录槽时不能为 GAP 再阻塞采集：在独立小型统计区累计，Storage 恢复时补写。
卡已经失败无法写 GAP 时，PC 仍依序号/CRC/缺 END 判 incomplete；内存统计只在
板仍供电且被读取时有意义，不能声称掉电后所有错误原因都可恢复。

END1 仅最终段，payload ≤16384B：截止 source 序号、会话窗口、observed_capture、
vision_processed、accepted、recorded、已确认同步边界、丢帧原因/数量、源头歧义、
storage_errors、状态和 error_reason。capture 数不可信时为 null 并提供可观察事件数。
written=成功写给文件系统的完整记录；先写 END，再 sync/close，任何后续失败都不
向操作者宣告 safe_to_remove。END 仅正常收尾完整且无丢失才能对应 complete。

## 6. 分段、预分配和掉电恢复

候选段上限 64MiB，在记录边界切换。大规模时间段不可使用 FAT32 单文件上限作
正常切段点。f_expand 可用于连续空间准备，但必须确认其**逻辑文件长度是否提前
扩展**；任何已分配而未实际写入的尾部都不能被解析成图像。

正常收尾定位 logical_end 并截断预分配尾部；异常掉电不能依赖此截断已发生。
PC 从头扫描，仅接受头 CRC、长度、几何、payload/record CRC 都通过的记录，
遇到未写空间/损坏尾部标 incomplete，不把预分配文件大小当作已写帧数。
旧簇残留恰好像合法记录仍存在风险；若使用预分配，应在协议冻结前明确每条记录
如何绑定当前 Session（例如记录中加入 session nonce，或分段区间/提交边界机制），
不能仅凭段头 UUID 消除旧尾记录混入。**该项未闭合前，不启用会暴露旧内容的预分配。**

源段缺失、重复索引、会话 UUID/配置冲突、倒序/重复 Frame ID、CRC 错误、无结束、
统计不一致分别报告。可恢复前缀不等于完整会话。正常取卡后在 PC 只读复制原字节，
禁止为方便而覆盖原卡文件或格式化。

## 7. PC adapter 实施要求

新增明确 version dispatch，保留旧 SCRAW-DRAFT-1 消费能力及 synthetic 标识。
正式 producer 和 PC parser 分别实现，使用独立 golden bytes、CRC 已知向量、
分段/截断/损坏/回绕/未知丢失样本交叉验证，不靠同一代码自序列化自解析放行。

原始目录/二进制为 A0；PC canonical Session/Frame 清单保留原身份、时间质量、
来源偏移和未知统计，不为满足旧 Schema 将 null capture count 编造为精确整数。
必要时升级 PC schema，保留旧版本，不修改全局测试 Runner 的契约。
掉电恢复允许生成独立 recovered manifest，不能假装原 session.json 已正常收尾。
