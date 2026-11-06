# SCRAW-DRAFT-1：PC 链条的暂定格式

日期：2026-11-04。**这不是已批准的车载 SCRAW 格式，也没有对应烧录固件。**
后续车载持续采集设计提出了独立的 [SCRAW v1 格式候选](FORMAT_V1_PROPOSAL.md)，
已有独立训练版 C/Python 实现，最终硬件冻结待完成；本页继续保留 PC synthetic 的旧
draft 约定，不改写历史字节格式。当前操作见[实施说明](IMPLEMENTATION.md)。
实现见 `tools/vision_data/scraw.py`。新鲜的 synthetic 测试可验证本字节约定，
真实上游格式冻结后应增加/替换 PC adapter，不能宣称已经兼容未知正式版本。

所有整数小端，无结构体自然对齐。CRC32 为 Python/zlib 的 IEEE CRC32，
输入初值 0，按字节计算；测试含标准 `123456789 -> cbf43926` 向量。

## 源会话

`session.json` 原样保留。版本 1.0，format=`SCRAW-DRAFT-1`；包含唯一 session_id、
明确 synthetic 标记、width/height/stride、GRAY8、timestamp_unit=`us`、事件语义、
firmware_git_sha（未知为 null）、camera、configured_fps、observed_capture_fps（未知
为 null），以及顺序 `000000.scraw`、`000001.scraw` 等完整分段名单。发现额外、
缺失或非连续名字即拒绝，不能忽略未列出的分段。不能从图像或接收时间猜测时钟语义。

## 每段头部

| 字段 | 字节数 | 约束 |
| --- | ---: | --- |
| magic | 8 | ASCII `SCRAWD01` |
| major | 2 | 1 |
| flags | 2 | 0 |
| metadata_length | 4 | 1..65536 |
| metadata_crc32 | 4 | 仅覆盖紧随的原始 UTF-8 metadata 字节 |
| metadata | 可变 | JSON；session_id、几何、像素格式、时戳语义、segment_index、format |

metadata 必须与源 session.json 的相应字段和分段索引完全相符。

## 记录

公共前缀：tag 4B、total_length u32；公共后缀：record_crc32 u32，覆盖**前缀和
整个 body**，不覆盖后缀本身。total_length 包含前缀、body、后缀。

FRM1 body：source_sequence u64、timestamp_us u64、payload_crc32 u32、GRAY8 payload。
长度为 `32 + stride × height`。payload_crc32 仅覆盖原始 payload，包含行 padding。
source_sequence 不重编号；本暂定版本约定会话内单调、非回绕 u64。当前板端 u32
回调计数将来怎样扩展/处理回绕，必须在正式采集格式中明确，PC 不猜测。

END1 body：UTF-8 JSON，包括 capture_frames、vision_processed_frames、recorded_frames
三种计数，以及可选 drops 分类。只有最终分段允许 END1。END1 只是源头声明的
正常结束，不是 PC 自行补写。计数必须非负，且记录数与恢复数一致。

记录 payload 上限 16MiB，END1 JSON 上限 64KiB。未知 magic/version/flags、
异常元数据或分段身份失败会拒绝导入，不自动解释成新格式。

## 恢复与状态

损坏记录按字节查找下一个候选标志，仅在长度、两层 CRC、固定几何均通过时接受。
标志落在像素里不等于新记录；不能仅靠 magic 切包。未恢复字节、截断、CRC 错误、
重复/倒序、缺口、非单调时戳、结束计数不一致分别报告。重复/倒序帧不覆盖先前帧；
原始损坏字节仍留在 A0。零帧为 invalid，有异常为 incomplete，否则为 complete。

CRC 是损坏检测而非密码学认证。无法仅凭序号缺口判断 Sensor、DMA、队列还是存储
丢失；工具保留源头分类计数并明确未知，不伪造原因。恢复会话可浏览，但默认禁止进入
可评测 Release。
