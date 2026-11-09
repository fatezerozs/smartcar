> **维护口径（2026-11-09）：** 主工程名保持MPC_20261106；本轮Phase2诊断是隔离源码生成构型，当前转弯软回调V2未替换生产固件/既有发布包。当前右轮间歇转动未解决、地面曲率未测；生成项目及AXF身份以Phase2交付页核对，历史镜像不自动升级。 统一状态与历史适用范围见[文档维护总览](../../../docs/DOCUMENTATION_STATUS.md)。

# 外设诊断工程

工程由仓库根目录的 `tools/build_motor_threshold.ps1` 与 `tools/build_link_mvp.ps1` 生成。
见 [工程入口](../README.md)。这些变体用于独立外设诊断，不能替代 competition 中的循迹固件。
