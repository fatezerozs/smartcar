# MPC_20261106：RT1064 视觉循迹小车

**2026-11-12 独立NEWNEW C26 V10归档：** [完整代码、固件与交接](docs/changes/2026-11-12_NEWNEW_C26_V10_HANDOFF.md)。保留C26；直道目标1.4m/s、实际环岛最高1.0、避砖0.55。Keil/所列Host通过，V10实车待验；独立NEWNEW不替换MPC主工程及其既往结论。

**2026-11-09进度更新：** [2026-11-09 当前进度与交接](docs/changes/phase3-progress-20261109/README.md)。当前处于Phase3影子上板诊断与软降级改造；V4已静态核对，near/full事件解耦已通过软件回归但未烧录。下方既往测试、板载身份和“未提交”叙述按各自时点保留；最新状态以该入口为准。

当前维护分支为 `main`；C32测试体系、MPC阶段成果、Recorder与标定资料已合入主线，合并基准为`00bb7a4`（2026-11-06）。
当前工程名为 **MPC_20261106**（MPC + 改动日期2026-11-06），见[工程更名记录](docs/changes/2026-11-06_MPC_PROJECT_RENAME.md)。
生产固件标识保持 C31：switch 为 `MIDTERM_C26_31`，独立 auto-track 为
`MIDTERM_C31_AUTO_TRACK_1`；本次工程归档不等同于正式烧录或实车发布。
主程序使用当前白色赛道几何、视觉转向和编码器轮速闭环。
速度环与编码器 3/4 已实车验证；入环、速度平滑及稳定过十字仍是当前任务，不能视为完整赛道已验收。

## 从哪里阅读

- **[影子验证待做清单](docs/changes/phase3-progress-20261109/README.md#待做清单推进有效运动影子验证)**：同帧原图断点、影子固件与配置、停机输入及导出、低速运动影子四步；主动接管定参与完整降级列为后续工作。
- **[全库文档状态与维护口径](docs/DOCUMENTATION_STATUS.md)**：当前成果、历史版本、工作区增量与各工程验收范围。

- **[Phase2实测成果与交接（2026-11-09）](docs/changes/phase2-calibration-20261108/DELIVERY_STATUS.md)**：落地响应、停车、无线与软回调成果，以及当前间歇转动问题和未完成标定。
- **[主要工程索引与交接导航](docs/PROJECT_INDEX.md)**：工程关系图、阶段进度、待解决问题、工程入口与接手路线。
- **[当前参数集与实测数据交付（2026-11-06）](docs/changes/vehicle-parameters-20261106/README.md)**：已采用的左右轮 CPR/方向、控制与舵机配置、几何参考、9 个 Encoder 原始窗口、Camera/SD 证据及联合候选待验状态。
- **[测试框架与流程详细手册](docs/TESTING_FRAMEWORK_GUIDE.md)**：环境、架构、套件开发、执行流程、报告判读、CI、数据集接入与完整套件索引。
- [C31 连续速度规划与降级控制阶段交付](docs/changes/c31-speed-planning-20261105/README.md)：[v0.3数据接入](docs/changes/c31-speed-planning-20261105/DATA_INPUTS.md)已消费实测CPR；[Phase 3首批安全边界改动](docs/changes/c31-speed-planning-20261105/PHASE3_BOUNDARIES.md)已进入源码并构建，速度规划器仍未接入生产控制路径。
- [项目设计与演进档案](docs/PROJECT_EVOLUTION.md)：成品目标、模型、实现与验证状态、任务、技术取舍和每次提交的简略索引。
- [主线合并与文档同步（2026-11-07）](docs/changes/2026-11-07_MAIN_DOCUMENTATION_SYNC.md)：main合并身份、远端CI、当前与历史记录的适用范围。
- [C32测试体系重构原始交付](docs/changes/2026-11-02_C32_TESTING_FRAMEWORK.md)：测试架构、兼容迁移及该版本验收快照。
- [文档目录](docs/INDEX.md)：当前参考、提交记录和历史归档。
- [构建与测试](tests/vision/README.md)、[版本固件与校验](releases/README.md)。

## 当前运行入口

工程已按用途分目录，见 [Keil 工程目录](demo/mdk/README.md)。原`competition/midterm_switch.uvprojx`已更名为`competition/MPC_20261106.uvprojx`。

Keil 打开 [MPC_20261106.uvprojx](demo/mdk/competition/MPC_20261106.uvprojx)，目标 `nor_sdram_zf_dtcm`。
在仓库根目录执行：

```powershell
.\tools\build_midterm.ps1 -Variant switch
```

此命令只构建，不烧录。工具链与设备包需在本机安装；通用/诊断变体的配置及产物不能混用。

**switch 固件保留 C26 启停：** ON 且连续五帧有效道路等条件满足后运行，OFF 停止。
独立 auto-track 当前为连续 1000ms 新鲜有效道路准入，默认 review=0 锁止；
板端/实车启动验收仍待完成。两种构型的差异见演进档案，不混用启动条件。
主工程图像调试模式会撤销运行输出。RecorderTrain另有行驶中持续采集的软件实现，稳定运动写卡、同步图文与完整收尾仍未验收。

AUTO_TRACK与RecorderTrain后续统一接入MPC预算降级；预算规划未集成前，当前C31实验路径仍采用零动力恢复保护，不把该过渡行为视为最终规划策略。[本轮审核与发布边界](docs/changes/2026-11-06_MPC_AUDIT_AND_PUBLICATION.md)

## 目录

| 目录 | 用途 |
| --- | --- |
| `demo/code/`、`demo/user/` | 应用调度、视觉、控制、执行器及板级接口 |
| `libraries/` | 保留的上游 SDK 与驱动 |
| `tests/vision/` | 主机回归、模拟 IO 与实拍夹具 |
| `tests/suites/`、`tests/support/`、`tests/benchmarks/` | 套件清单、公共夹具与待接入的数据基准契约 |
| `tools/` | 构建、采集、导入、训练和分析 |
| `docs/` | 演进档案、提交说明、参考与历史资料 |
| `releases/`、`evidence/` | 明确版本的发布产物/校验与可复核证据 |

选定的Phase2原始串口与采样证据已随文档归档；外部大视频、临时构建和个人IDE状态保留本地，发布目录只归档明确选定版本。
测试统一入口为 `tools/testing/Invoke-TestPlan.ps1`；使用说明及证据见
[测试体系](docs/TESTING_ARCHITECTURE.md) 和 [验收记录](docs/testing/VALIDATION.md)。
main合并提交的[MSVC optimized/ASan CI](https://github.com/fatezerozs/smartcar/actions/runs/42243602219)各49 PASS / 1 SKIPPED；此前完整本地PreFlash为63 PASS / 1 SKIPPED。跳过项是缺数据的`performance-host`，正式烧录放行仍单独判定。
主机通过不等于实车不出界、入环或完整覆盖验收。
第三方许可证保持原样，见 [libraries/LICENSE](libraries/LICENSE) 和各文件头。
