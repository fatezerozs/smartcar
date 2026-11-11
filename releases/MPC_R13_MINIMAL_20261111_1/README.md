# R13 最小赛道工程：首版软件候选

2026-11-11，北京时间。`MPC_R13_MINIMAL_20261111_1` 已完成本轮软件实现、主机检查、ARM 构建和源码封存，**未烧录、未复位、未发车，完整赛道尚未验收**。

实际链路是 R13 视觉控制、配对的 R13 轮速目标与 PI、C26 生命周期及原最终输出；保留将来替换规划算法的职责接口。米制规划和影子运行关闭，尚无 MPC 求解器。独立候选未覆盖根目录 `demo/code/` 中的其他工作。

## 本轮结果

| 检查 | 结果及范围 |
| --- | --- |
| 完整 GCC Host | 70 PASS、0 FAIL、1 SKIPPED，95.4 秒；覆盖当前主工作树，发生在审查修正前。修正后重新检查了独立候选。 |
| 最终候选专项 | 统一入口 3 PASS（含两项环境）；内部 6 项通过。灰度环形 56 次检查；R13 完整链 28 组构造场景、10,480 步；启动保护 81 组场景、22,474 次检查。 |
| R13 输出对照 | 10,472 行完全一致；8 行为已知无道路时的缓存清零差异。10,480 步最终电机 PWM、舵机命令均与冻结 R13 相同。输入为构造道路/编码器序列，不是实车轨迹。 |
| 最终 Keil 构建 | `nor_sdram_zf_dtcm`，0 错误、0 警告；保存 AXF、BIN、MAP、反汇编与哈希。 |
| 源码包独立复现 | 解包核对 745 个源文件，6 项专项和重新生成的 Keil 构建通过，不依赖历史发布 ZIP 或原 `.build`。BIN 仅编译时间字符串的 4 个字节不同，其余字节一致；两份完整哈希均记录。 |
| 双轴审查 | Standards、Spec 当前均无未解决的可确认代码问题；实车与完整路线要求仍未验收。 |

完整 Host 跳过 `performance-host`：其数据集、版本化真值、测量适配器和批准基线尚未齐备。独立图像性能检查虽通过，使用的是当前主工作树，不能把其速度/识别指标移用于本候选。Host 耗时不是板端控制耗时。

[完整 Host 中文报告](../../.test-results/20261111T153404-4e5ab5fd25/user-report.md) · [最终候选中文报告](validation-final/user-report.md) · [源码包专项输出](archive-verify-0.log) · [源码包构建输出](archive-verify-1.log) · [独立复现核对](archive-verification.json)

## 环形与审查修正

新增当前外侧弧线资格、连续出口证据、出环后普通道路重获及两次视觉事件预算；预算用于已知路线辅助，不按时间或轮程直接生成转弯。完成两次后抑制第三次入环，C26 OFF/新运行清零，没有新增图像、二值图或路径录制。

审查中发现并处理了三项问题：

- 相机历史重置误清任务进度：保留已完成计数，只有 OFF/新任务清零。公共灰度测试先复现失败，再验证修正通过。[失败记录](history-reset-red.log)
- 仅远处道路可见、最终路径无效时误记出环完成：完成事件延至最终路径有效后提交，无效帧打断连续确认。公共灰度反例先失败后通过。[失败记录](exit-reacquisition-red.log)
- 部分输出仍显示旧版本：统一构建身份，并移除 C26 下与实际启动方式冲突的帮助文字。

Standards 轴原有 2 项判断性发现已修正；Spec 轴确认的 2 项行为问题已修正，其中历史重置问题重叠。另有负向验证：临时删除完成事件后，同一公共灰度测试确实失败，生产源码没有保留该变异。

**两次视觉事件不能证明中央和右上两处各绕完整一圈。** 当前没有物理位置与绕行角度真值；误识别仍可能消耗预算。普通道路、S 弯、两处环形各一次进出、返回 A 的连续实车全程，以及启动保护候选门槛，都需要后续验证。自动识别 A 并停车不在本轮承诺内。

## 工程入口与来源

普通构建：[MPC_R13_Minimal.uvprojx](../../.build/mpc-r13-minimal-20261111-r2/workspace/demo/mdk/competition/MPC_R13_Minimal.uvprojx)

下载工程：[MPC_R13_Minimal_Download.uvprojx](../../.build/mpc-r13-minimal-20261111-r2/workspace/demo/mdk/competition/MPC_R13_Minimal_Download.uvprojx)

完整 Windows 路径：`C:\shengchanli\smartCar\smartcar\.build\mpc-r13-minimal-20261111-r2\workspace\demo\mdk\competition\MPC_R13_Minimal_Download.uvprojx`；Target：`nor_sdram_zf_dtcm`。本轮仅准备入口，未执行下载。

应用基线为冻结的 `R13_V2_START_GUARD_20261111_1`，其启动保护仍未实车验证。源自该版本的 73 个应用文件中，本轮改动 6 个；共享输入锁定 550 个。新版本化源码、职责接口及再生方法见[候选说明](../../demo/candidates/minimal-track/README.md)，计划范围见[提案](../../docs/changes/planner-authority-design-20261111/MINIMAL_TRACK_PROPOSAL.md)。不继承 R4 的一秒截止、一次加载限制；不宣称 ADR-0001 的米制 CONTROL 已完成。

可再生源码：[source-code.zip](source-code.zip)；解包后先按候选说明重新生成工程。ZIP 中现成工程的输出路径指向本机原构建目录，异地或独立副本应重新生成。首次封包复现因遗漏离线测试导入依赖而失败，补齐 `verify_b3_static.py` 后通过；[初次失败记录](archive-verify-initial-missing-dependency.log)保留，不涉及固件算法变更。

## 详细证据与边界

[源码清单](source-manifest.json) · [候选构型与源码/产物身份](candidate.json) · [范围与大小](scope-audit.json) · [相对 START_GUARD 的应用补丁](changes-from-start-guard.patch) · [双轴审查](REVIEW.md) · [产物哈希](SHA256.json) · [构建日志](build.log) · [链接布局](firmware.map)

链接器记录 ROM 108,152 字节、RW+ZI 220,360 字节。它们是当前布局的静态占用，不代表栈峰值或板端时序；没有相同条件下的体积对照，不报告缩小比例。轻量化主要体现在单条实际决策链、关闭规划/影子运行及移除实验启动限制，不宣称已压到最小源码或 RAM。

读取层：计划 Markdown 直接阅读；C、XML、JSON 为 `structured_parsed`；编译、灰度输入回归、差分、哈希及源码包复现为 `computed`。没有仅基于 `md_extracted` 的关键结论，没有新增 `pdf_verified`、物理测量或实车轨迹证据。

本轮修改范围为独立候选目录、生成器、专项测试与冻结参考、catalog 的 `minimal-track` 条目、本报告及提案状态。候选和发布目录的 `.gitattributes` 仅保护哈希证据的原始字节。未修改全局配置、skills、AGENTS.md 或持久 Agent 行为；未覆盖用户其他未提交改动。
