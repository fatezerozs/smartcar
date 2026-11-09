> **维护口径（2026-11-09）：** Phase2实测与诊断工具已单独发布于bd94f27；没有据此验收Recorder运动写卡、Camera/SD/PI组合或正式训练数据集。现有静止/软件结果按本文原构型阅读；其他未提交Recorder/Phase3实现不能写成main已发布或当前车载配置。 统一状态与历史适用范围见[文档维护总览](../DOCUMENTATION_STATUS.md)。

# 上下游兼容性（2026-11-04）

| 边界 | 当前处理 | 不可推断 |
| --- | --- | --- |
| 车载 SCRAW → PC | 新增 SCRAW-1 训练版 C/Python 实现；旧 SCRAW-DRAFT-1 仍需 `--allow-draft` | C/Mock 通过不等于真实卡/持续行驶通过，最终格式冻结待验 |
| A0 → A1 | A0 保留 `.scraw` 和 `session.json` 原字节；A1 按 stride 提取可见行，无缩放/裁剪/滤波 | 不支持把 94×60 预览放大成 188×120 原始输入 |
| A1 → C31 | 仅接受 188×120；编译当前 `vision_midterm.c`，宏在 run.json 记录 | 不重现执行器、Bayes 调度、摄像头/MCU/控制闭环 |
| A0 → A2 | 无损 GRAY8 PNG；导入/验证逐像素检查，源头 padding 保留在 A0 | A2 文件不是原始传感器曝光时间证据 |
| Canonical → Label Studio | 任务 JSON、Choices、KeyPointLabels 与版本化 sidecar；只导出选定图像 | 未核验真实实例保留 sidecar/point metadata 的能力 |
| Label Studio → Canonical | 外部 ID 由 instance/project/task/annotation 命名空间化；原导出快照哈希保留 | completed、转换成功或 QC 不等于人工 approved |
| T3 | Canonical PNG Mask 使用 0/1/2/255；topology 只允许 image_observation | 不自动推测 brush RLE、route_intent 或未标注全背景 |
| C31 → Prediction | 独立机器记录，绑定输入、宏、Git、run 和配置；不得导入为人工标注 | 模型置信度和机器结果不能成为自己的真值 |
| Dataset → C32 | 独立 `vision_data.py evaluate` 消费者；产出锁定 Release 和 run.json/summary | 现有 benchmark.py 的仓库相对路径不能直接指向仓库外原始数据 |

本轮保持 C32 Runner、CI、Vision Equivalence、Field Protocol 和 Macro Contract 原样。
新消费者从仓库外数据根读取；不会通过软链接或复制大数据进 Git 来规避路径边界。
若未来要在 C32 Profile 注册消费者，应作为单独配置增量审查，不重写全局执行内核。

### 身份与序列

Frame ID=`session_id:source_sequence`；Session ID 由源清单声明，不以导入时间替换。
清单保留原序号、us 时间、事件语义、分段/偏移/长度、两层 CRC/哈希和 A1 变换。
全序列保留，抽样只引用 ID。Frame 模式每帧 reset，Sequence 模式按 Session/序号
缺口/时间不连续 reset；时间间隔阈值 250000us 对应当前 app_runtime 的历史清理
常量（源 `HISTORY_GAP_MS=250`），不是重放控制周期或传感器实时性证明。

同一 Session 只有一个 split；exact pixel SHA256 跨任何 split 均拒绝，近重复
8×8 average-hash 候选和 track layout 跨分区报告保留。近重复是诊断启发式而非
零泄漏证明；共享道路背景和连续帧相关性仍须人工检查。

### 人工真值

审核日志与提交分离；必须显式 `reviewed` 再 `approved`，记录独立 reviewer、时间、
说明、对象哈希、前一记录哈希。不认证真人身份、不签名；声明操作由实际操作者负责。
Release 冻结审核快照，后续审核不会静默改变旧 Release。同帧多个 approved 结果
要求先裁决，不随意挑一份。Holdout 不展示 Prediction；被 C31 影响或保留预测血缘
的标注不得用于评价 C31。仅有 T1/没有匹配几何或 Mask 的帧不伪造几何准确率。
