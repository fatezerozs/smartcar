# Keil 工程入口

当前实车循迹使用 **competition/midterm_switch.uvprojx**。迁移日期：2026-10-23。
各目录位于同一深度，共用上级 `scf/` 链接脚本、`ini/` 调试初始化及仓库源码。

| 目录 | 用途 | 生成 / 构建（仓库根目录执行） |
| --- | --- | --- |
| `competition/` | 当前 C31 主程序 | `./tools/build_midterm.ps1 -Variant switch` |
| `base/` | 基础工程及变体生成模板 `rt1064.uvprojx` | `./tools/build_keil.ps1 -UV4 E:/ElecTronic/Keil/UV4/UV4.exe` |
| `training/` | route、train、auto 训练与实验变体 | `./tools/build_midterm.ps1 -Variant route`（或 train / auto） |
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
