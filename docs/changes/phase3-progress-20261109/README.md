# Phase3当前进度与交接

2026-11-09（北京时间）。本页是此次GitHub main进度归档的持续入口，统一源码、板载与历史阶段报告的适用范围。[项目主页](../../../README.md) · [工程索引](../../PROJECT_INDEX.md) · [旧C31任务入口](../c31-speed-planning-20261105/README.md)。

## 当前阶段

Phase2部分完成；Phase3处于影子模式上板诊断与连续降级适配阶段。可选米制运行链路已接线并有软件验证，主C26候选以影子模式验证，新规划尚未取得生产动力控制验收。Phase4完整追溯与赛道回归尚未完成。

| 层次 | 当前结果 | 剩余工作 |
| --- | --- | --- |
| Phase2数据 | 局部直线响应、停车、尺度及映射候选已有，见[交接](../phase2-calibration-20261108/DELIVERY_STATUS.md) | 动态时延、运动同步、适用误差与生产边界核定；转弯连续性与地面曲率 |
| Phase3软件 | 原生计数/视觉输入、监督规划、可选原PI目标源与唯一writer已接线；默认主动控制关闭 | 生产配置、并行时序、有效运动输入与闭环验收 |
| 最新板载 | PHASE3_C26_SHADOW_20261109_4 / C26-SHADOW-8CA1CE06，见[V4静态证据](../phase3-v4-boardcheck-20261109/README.md) | 停机图像缺54/52行，304个观察帧near/full米制均未准入；需同帧原图及识别状态回读定位 |
| 最新源码增量 | [near/full事件解耦](../soft-degradation-events-20261109/README.md)已完成，专项回归通过；尚未烧录 | 可变路径段、残差权重、受限能力融合与基于可停性的恢复尚待实施 |
| 追溯 | [能力核查](../phase3-v4-boardcheck-20261109/TRACEABILITY_AUDIT.md)：部分RAM/串口诊断可用 | 缺统一Session、逐图时间关联与每次行驶自动持久保存 |

## 本轮归档范围

归档此前工作区的C26快照提交、规划接口/影子/运行时、米制输入与PI适配、V2/V3/V4板测及原图回归、near/full事件解耦、Recorder联合静止与运动工具进展。各阶段只对其注明的构型和输入负责，Recorder独立能力不等于V4已具备行驶录像。

- [运行链路](../phase3-runtime-20261109/README.md)与[影子构建/候选准备](../phase3-shadow-preparation-20261109/README.md)
- [V3真实输入审计](../phase3-real-input-20261109/README.md)、[V4原图修复](../phase3-vision-regression-20261109/README.md)、[V4停机核对](../phase3-v4-boardcheck-20261109/README.md)
- [联合静止测量](../recorder-joint-static-20261108/README.md)与[运动工具交接](../recorder-motion-20261108/README.md)
- [连续降级方案原文](../soft-degradation-events-20261109/SOFT_DEGRADATION_PLAN.md)与[已实现事件子集](../soft-degradation-events-20261109/README.md)

归档源码不自动更新现有AXF或主板。软件构建候选、板上V4和此次事件解耦源码是不同版本；此前文档“未提交/未合入/尚未上板”保留为阶段记录，当前状态以本页及对应最新原始证据为准。不会因维护日期改变工程名称MPC_20261106。

## 下一步

1. 导出同一帧188×120原图、边界/阈值、帧号及采集/处理时间，复现板上几何缺失。
2. 基于真实可用路径、标定适用性与运动证据推进连续能力约束；当前恢复期限按方案保留，禁止凭静止帧选定生产停车/恢复参数。
3. 先做构建与停机回归，再验证有效影子输入、控制并行和实车闭环；补齐每次行驶独立追溯。

## 文档与证据约定

旧入口持续维护，历史证据不改写。`.build`与`.test-results`是本地生成目录；本轮关键测试摘要和事件审计已复制到文档evidence目录，冻结报告中的原构建路径仅用于定位当时环境。已有阶段evidence目录保留原始哈希与边界，未将预览放大结果当作同帧原图真值。

源码/JSON/Keil配置为structured_parsed，测试与统计为computed。未新增pdf_verified或仅依赖md_extracted的关键数值结论。本轮不修改全局配置、技能或持久Agent行为。

## 本次发布前软件验证

完整GCC Host回归59 PASS / 1 SKIPPED，见[完整报告](evidence/host/summary.md)及[机器记录](evidence/host/run.json)。跳过performance-host的原因仍为缺少版本化数据集、Ground Truth、测量适配器及批准基线；没有本轮ARM目标构建、MSVC/ASan或实车放行结论。测试记录保留执行时HEAD与dirty身份，随后仅维护文档和归档证据。
