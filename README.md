# Smartcar：RT1064 视觉循迹小车

当前维护分支为 `main`，工作版本为 **MIDTERM_C26_31**。
主程序使用当前白色赛道几何、视觉转向和编码器轮速闭环。
速度环与编码器 3/4 已实车验证；入环、速度平滑及稳定过十字仍是当前任务，不能视为完整赛道已验收。

## 从哪里阅读

- [项目设计与演进档案](docs/PROJECT_EVOLUTION.md)：成品目标、模型、实现与验证状态、任务、技术取舍和每次提交的简略索引。
- [本次详细提交说明：2026-10-23 / C31](docs/changes/2026-10-23_C31.md)：C29—C31 的改动、证据与限制。
- [文档目录](docs/INDEX.md)：当前参考、提交记录和历史归档。
- [构建与测试](tests/vision/README.md)、[版本固件与校验](releases/README.md)。

## 当前运行入口

Keil 打开 [midterm_switch.uvprojx](demo/mdk/midterm_switch.uvprojx)，目标 `nor_sdram_zf_dtcm`。
在仓库根目录执行：

```powershell
.\tools\build_midterm.ps1 -Variant switch
```

此命令只构建，不烧录。工具链与设备包需在本机安装；通用/诊断变体的配置及产物不能混用。

**C31 仍使用 C26 启停：** ON 且连续五帧有效道路等条件满足后运行，OFF 停止。
“上电识别正确赛道若干秒后启动、不依赖 C26”是待实现目标。
图像调试模式会撤销运行输出；行驶中图文同时采集尚未实现。

## 目录

| 目录 | 用途 |
| --- | --- |
| `demo/code/`、`demo/user/` | 应用调度、视觉、控制、执行器及板级接口 |
| `libraries/` | 保留的上游 SDK 与驱动 |
| `tests/vision/` | 主机回归、模拟 IO 与实拍夹具 |
| `tools/` | 构建、采集、导入、训练和分析 |
| `docs/` | 演进档案、提交说明、参考与历史资料 |
| `releases/`、`evidence/` | 明确版本的发布产物/校验与可复核证据 |

原始串口、外部视频、临时构建和个人 IDE 状态保留本地；发布目录只归档明确选定的版本。
主机通过不等于实车不出界、入环或完整覆盖验收。
第三方许可证保持原样，见 [libraries/LICENSE](libraries/LICENSE) 和各文件头。
