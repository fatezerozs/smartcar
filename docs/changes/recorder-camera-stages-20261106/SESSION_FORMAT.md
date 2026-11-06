# SCRAW-1 连续采集契约与 PC 增补

本轮不更改SCRAW-1 wire版本，沿用已有C生产者和独立Python消费者。新continuous_report是PC侧派生报告，不写回原始SD文件。

## 文件与记录布局（little endian）

64B分段头：0..7 `SCRAW1\r\n`；8/10 major/minor uint16=1/0；12 header_size uint16=64；14 flags=0；16 segment_index uint32；20/22/24 width/height/stride uint16；26 pixel-format uint16=1（GRAY8）；28 time-unit uint8=1（us）；29 timestamp-event uint8=1（CSI IRQ GPT1 ms×1000）；30 reserved uint16=0；32..47 UUID16B；48 metadata_size uint32；52 metadata_crc uint32；56 reserved uint32=0；60 header_crc覆盖前60B。紧随UTF-8 JSON，读前先检查头CRC和长度，meta上限4096B。

记录：0 tag4B（FRM1/GAP1/END1）；4 total_length uint32；8 source_seq uint64；16 timestamp_us uint64；24 payload_size uint32；28 payload_crc uint32；32 flags uint32（FRM=0，GAP/END=1）；36 header_crc覆盖前36B；40开始payload；末尾record_crc覆盖40B头+payload。总长=payload+44。CRC32为IEEE反射，C rec_crc_update 与PC zlib独立验证。
FRM payload=stride×height，保留完整原始padding；显示PNG只提取width有效列，不resize。当前配置width=stride188,height120，不代表传感器实测规格。GRAY8为单字节，无多字节像素序；有效区为[0,0,width,height]。曝光/gain/FPS配置与actual readback分开，UNKNOWN保持null。

GAP/END不绑定source帧，其seq/time为0。GAP累计delta、unknown_loss与可能为null的源范围保留，不能据此伪造精确物理丢失帧数。END的observed_capture是已观察完成事件，不是精确sensor capture_frames；后者必须null。完成判据需要帧/accepted/observed计数、零drop/ambiguity、首末边界、时间戳、CRC和完整清单一致。
Writer正常停止先Capture ack关闭→有界drain→GAP/END→sync/close→CREATE_NEW session.json→unmount。只有最终manifest和END都有效仍不足以忽略计数/序列问题。缺/坏manifest可按校验过的分段头扫描恢复，但必为incomplete；不补零帧、不猜造结束标记。

## 本轮新增PC增补

archive增加实际首末source_seq与END/base_source的一致性检查；CRC合法的错误END也不能complete。continuous输出全量frame_timeline.csv（frame_id、capture完成时刻、几何、文件偏移、payload大小/CRC/SHA）、missing_frames.json（范围和unknown标志）、连续性/FPS/帧间隔统计、完整性与来源状态。
source_sequence较大时缺口保存范围，不展开可能巨大的ID列表。frame_id为SessionID:source_seq，64位序号/时间戳不截断到32位；Camera真实丢失数量在歧义时仍未知。observed_valid_record_fps只从有效记录时间跨度计算；缺口/不连续时不能当真实传感器输出帧率。
Label Studio经现有sampling.make_view与annotations.make_tasks产生独立抽样资产、tasks.json、来源hash和上下文；原A0字节、全量A2灰度帧及frames.jsonl完整保存。抽样不改变complete/incomplete，也不授予人工标注/放行。

不实现B3遥测sidecar，不以离线文件标志代替硬件验收：telemetry_available=false、b2_hardware_accepted=false、production_ready=false。新格式或遥测记录以后需单独版本化，不能让旧reader静默忽略关键字段。

## B2实测补充

SCRAW-1未改版；B2METRIC为关闭后串口测量，不写回原始SD Session。R0000003输出188×120/GRAY8、stride188，CSI控制器依现有驱动以240×94周期分组接收，总字节同为22560，不能直接把控制器分组当成图像的宽高。捕获100事件软停止本次保留101帧，END/manifest及所有实际序号必须一致，不截去额外合法帧凑100。
