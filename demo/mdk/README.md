> **维护口径（2026-11-09）：** 主工程名保持MPC_20261106；本轮Phase2诊断是隔离源码生成构型，当前转弯软回调V2未替换生产固件/既有发布包。当前右轮间歇转动未解决、地面曲率未测；生成项目及AXF身份以Phase2交付页核对，历史镜像不自动升级。 统一状态与历史适用范围见[文档维护总览](../../docs/DOCUMENTATION_STATUS.md)。

# Keil 工程入口

主工程、采集/静止测量工程与各自说明统一见[主要工程索引](../../docs/PROJECT_INDEX.md)。本页补充Keil变体目录和生成方式。

维护：2026-11-07。以上工程源码及生成工具已合入main（`00bb7a4`）；进入同一仓库分支不改变各构型的任务、宏、启动授权或验收范围。[主线合并与CI](../../docs/changes/2026-11-07_MAIN_DOCUMENTATION_SYNC.md)

当前主工程名为 **MPC_20261106**，入口为 [competition/MPC_20261106.uvprojx](competition/MPC_20261106.uvprojx)。2026-11-06由`midterm_switch.uvprojx`更名，日期后缀为本次改动日期，见[更名记录](../../docs/changes/2026-11-06_MPC_PROJECT_RENAME.md)。
采集训练构型包含连续1秒新鲜道路准入、UART损坏字节过滤及SD单数据线/引脚隔离修正；默认运动审核为0、蜂鸣器静音。AUTO_TRACK和RecorderTrain后续统一接入MPC预算降级；当前共享C31实验路径仍是预算未接入前的零动力恢复保护，旧rev5“近场有效可持续驱动”说明不是当前源码保证。
先通过`tools/build_recorder.ps1`生成`training/recorder_train.uvprojx`及私有适配源，该生成工程不随Git提交。产物位于`.build/recorder-train/Objects/recorder_train.axf`；运动中持续记录仍待验收，不构成烧录放行。
默认`./tools/build_recorder.ps1`生成运动锁定构型；`-MotionReviewed`只用于另有对应审核依据的实验构建。
各目录位于同一深度，共用上级 `scf/` 链接脚本、`ini/` 调试初始化及仓库源码。

| 目录 | 用途 | 生成 / 构建（仓库根目录执行） |
| --- | --- | --- |
| `competition/` | 当前 C31 主程序 | `./tools/build_midterm.ps1 -Variant switch` |
| `base/` | 基础工程及变体生成模板 `rt1064.uvprojx` | `./tools/build_keil.ps1 -UV4 E:/ElecTronic/Keil/UV4/UV4.exe` |
| `training/` | route、train、auto 训练与实验变体 | `./tools/build_midterm.ps1 -Variant route`（或 train / auto） |
| `training/recorder_train.uvprojx` | 独立采集训练 RTOS/持续 SD 写入，默认运动锁定 | `./tools/build_recorder.ps1`；先读 [实施说明](../../docs/vision_recorder/IMPLEMENTATION.md) |
| `diagnostics/` | 编码器速度环、运动、电机阈值与链路诊断 | `./tools/build_motor_threshold.ps1 -Variant speed`（或 motion / threshold）；`./tools/build_link_mvp.ps1` |

`base` 是共同配置来源；生成的变体不要单独修改编译宏来维护另一份配置。
主程序和 route 工程随仓库保留，其他变体由上述命令生成，避免多份过期工程成为入口。
所有构建产物写入仓库 `.build/<变体>/`，工程目录不存放 Objects / Listings。
脚本只构建，不自动下载。IDE 调试设置 `.uvoptx` 仅本地保存，不提交；本次随工程迁移保留已有设置。

历史 `releases/*/release.json` 中的工程路径与哈希是当时发布的证据，不随目录迁移改写。
IAR 工程仍在 `demo/iar/`，属于上游参考，未作为当前 C31 的构建入口验证。

迁移验证：switch、route、encoder-speed、base 四个工程重新构建，均为 0 错误、0 警告；
九个工程共 3681 个源码、包含目录及链接脚本引用均可解析。
三份受版本控制工程的编译宏、优化设置及设备选择与迁移前一致。
路径改变会影响嵌入文件路径的诊断字符串和调试信息，新构建不应与历史发布固件直接比较文件哈希。
