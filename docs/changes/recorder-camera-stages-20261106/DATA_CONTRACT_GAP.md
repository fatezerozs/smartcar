# 数据契约覆盖与缺口

沿用SCRAW-1，不另建不兼容wire协议。Session ID/UUID、分段几何、GRAY8、source_sequence、completion timestamp、payload/header/record CRC、GAP和END已覆盖。

| 所需数据 | 当前持久化来源 | 状态 |
| --- | --- | --- |
| 灰度原始帧/宽高stride | FRM1 + segment header / metadata | 可复用；配置不代表实际相机读回 |
| 帧ID/文件偏移 | Session ID + uint64 seq；PC frames.jsonl | 可复用；每帧可追溯到原文件 |
| 捕获时刻 | CSI IRQ读取GPT1毫秒，扩展wrap后×1000 | 分辨率1000us；是完成事件，不是曝光起点 |
| 丢帧/歧义 | GAP1累计计数，END counters；PC推断序号范围 | 内部缺口可定位；GAP源头first/last常为null，歧义不能恢复真实帧数 |
| 曝光/gain/FPS | configured_sensor_fps/exposure/gain；actual_exposure_readback=null | 仅配置值，实际参数UNKNOWN |
| Encoder原始counts与采样时刻 | board_encoder_read / vd_speed_sample_tick，仅RAM/诊断 | 未写进SCRAW |
| 实际wheel ticks/左右目标 | vision_speed / recorder_drive_status | 未形成同源逐样本sidecar |
| PI输出与最终PWM提交 | vision_speed + vision_actuator最终提交 | 必须分别记录，当前未持久化 |
| 舵机命令/实际转角 | 命令RAM；实际角无传感证据 | 未持久化；命令不得称实测角 |
| C26开关/授权/故障转移 | 主线逻辑/诊断 | Recorder训练启动语义不同，未形成主线事件契约 |
| battery/工况/calibration_version | 无本车记录 | UNKNOWN，不填示例值 |

B2只验Camera连续序列，不虚构运动遥测。新continuous_report明确 telemetry_available=false、b2_hardware_accepted=false、production_ready=false。
B3前必须批准版本化sidecar：capture_seq/capture_ts_us；sample_seq/sample_ts_us及raw_left/raw_right/counter_bits；control_seq/control_ts_us及target/measured/PI；commit_seq/commit_ts_us及最终PWM/servo_us；C26事件/fault/state/有效性位。所有流定义同一个时钟epoch、分辨率和overflow；异步采样只能做显式nearest/previous join并记录age，不能把目标冒充实际值或把最近UART状态冒充同帧遥测。
sidecar未在本轮实现，不能把新增离线报告当作该契约已满足。具体兼容升级应使用新可识别记录/sidecar版本，旧reader遇未知版本拒绝；不能悄悄往FRM payload后塞字段。当前已冻结的是原SCRAW-1和新的PC质量报告schema。
