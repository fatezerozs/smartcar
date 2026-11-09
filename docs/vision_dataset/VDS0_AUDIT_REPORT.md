> **维护口径（2026-11-09）：** Phase2实测与诊断工具已单独发布于bd94f27；没有据此验收Recorder运动写卡、Camera/SD/PI组合或正式训练数据集。现有静止/软件结果按本文原构型阅读；其他未提交Recorder/Phase3实现不能写成main已发布或当前车载配置。 统一状态与历史适用范围见[文档维护总览](../DOCUMENTATION_STATUS.md)。

# VDS-0 现状与边界（2026-11-04）

基线、提交日期、固件事实见 [采集基线审计](../vision_recorder/C31_BASELINE_AUDIT.md)。
本轮用户在澄清中选择“完成 PC 侧软件链条”，因此主计划原来的“审计完成即停止”
被本轮明确范围替代；固件、硬件、真实 Label Studio 写入、全局测试编排不在本轮实施内。

## 审计结果

| 对象 | 事实/证据 | 结论 |
| --- | --- | --- |
| C32 下游 | `tools/testing/benchmark.py:validate_benchmark`；`tests/benchmarks/README.md` | 存在带批准基线、质量/速度独立门禁的消费者接口；现有数据路径约束为仓库内 |
| 旧图传导出 | `tools/capture_vision.ps1:Pop-VisionPacket,Save-VisionImage`；`tests/vision/replay_preview.c:main` | 94×60 预览、停车/离线重放用途；不能作为 188×120 原始帧 |
| 生产输入 | `demo/code/vision_frame.c:20`；`app_runtime.c:277` | 原始灰度复制到 188×120 前台缓冲后进入视觉；没有先缩放的 PC 可复现变换 |
| 生产复用 | `demo/code/vision_midterm.h:vision_midterm_process`、`vision_midterm_reset` | 模块纯视觉，可单独 Host 链接；本轮 adapter 不链接车辆执行器 |
| 原始会话 | 仓库无 Recorder 或正式 SCRAW 导出器 | 没有可声明为真实连续 Capture 的数据；SCRAW 正式兼容性待冻结格式 |
| 数据目录 | 工具要求用户显式 `--data-root`，解析真实路径后拒绝落在 Git 内 | 没有擅自绑定个人数据目录；本轮只生成仓库外 synthetic 演示 |
| Label Studio | PATH 未找到 conda/label-studio；没有读取凭据、查询实例或扫描用户环境 | 环境名、版本、端口、项目、API 能力均未知，不能说“未安装”或“可写” |

## 本轮能力和限制

PC 标准库工具从暂定源会话导入，保存 A0、帧/会话清单、A1 对应和 A2 PNG，完成
抽样/事件上下文、人工文件交换、独立审核 ledger、Session 切分、Release 锁定、生产
C31 单帧/序列运行及比较。附独立 synthetic/mock 验证，不改现有 Runner/CI。

Label Studio 采用文件接口和 mock transport；没有实现/启用真实实例 HTTP 写操作。
可选素材服务仅绑定 127.0.0.1，服务明确选定的哈希锁定 PNG；没有目录列举、任意路径
或写请求。只有显式 `serve` 命令才启动，本轮验收不连接已有 Label Studio。

T1 Choices、T2 KeyPointLabels 投影可做文件往返；开放折线通过有序点组和 Canonical
sidecar 保存，不假装成闭合 Polygon。T3 GRAY8 Mask 和 image_observation topology
由 sidecar 引用；不猜测原生 brush RLE。元数据丢失或不支持结果类型会拒绝转换，
真实实例的导入/编辑/再导出兼容性仍待小批试点。

## 证据层级

Markdown/源码直读；本轮 JSON/工程 XML 为 `structured_parsed`，实际工具运行、
CRC/哈希/像素一致性为 `computed`，无仅靠 `md_extracted` 的关键数字或
`pdf_verified` 声明。没有真实素材、人类审核、实车、MCU 时序或正式性能基线证据。
