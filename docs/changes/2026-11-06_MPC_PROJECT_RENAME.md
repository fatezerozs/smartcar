# 工程更名：MPC_20261106

日期：2026-11-06，Asia/Shanghai。按用户要求，以`MPC`作为工程名、以改动日期`20261106`作为后缀，当前主工程命名为 **MPC_20261106**。

## 当前入口

Keil打开 [MPC_20261106.uvprojx](../../demo/mdk/competition/MPC_20261106.uvprojx)，目标仍为硬件/链接布局对应的`nor_sdram_zf_dtcm`。

```powershell
# 仓库根目录；switch保留为既有构建参数，生成/构建新名称工程。
pwsh -NoProfile -File tools/build_midterm.ps1 -Variant switch
```

| 对象 | 更名前 | 更名后 |
| --- | --- | --- |
| 当前工程名 | Smartcar / midterm_switch入口 | MPC_20261106 |
| 主Keil工程 | `demo/mdk/competition/midterm_switch.uvprojx` | `demo/mdk/competition/MPC_20261106.uvprojx` |
| 本地IDE调试选项 | `midterm_switch.uvoptx` | `MPC_20261106.uvoptx`，原字节保留 |
| 本地IDE界面选项 | `midterm_switch.uvguix.zfn19` | `MPC_20261106.uvguix.zfn19`，原字节保留 |

主页README标题、Keil目录、项目演进档案、文档目录和Phase 3当前工程链接已同步。历史报告/任务书中的旧名称按当时记录理解；旧发布产物、参数交付MANIFEST及已有构建/测试证据不追溯改名。

## 构建与工具同步

- `tools/build_midterm.ps1 -Variant switch`现在生成新文件名；其余变体仍按原有名称生成。
- `tools/speed_planning/audit_build.py`按新名称隔离复制并构建实际工程。
- Recorder阶段审计、B3静止/联合候选生成与验证工具的主工程路径共6个Python消费者已更新；原有源码锁、候选哈希和硬件验收边界保留。

本次工程文件只改变文件名，XML字节与更名前一致。因此硬件目标、宏、链接配置、源文件集合、调试接口与原构建钩子配置未变。兼容现有构建/产物消费者，输出目录仍为`.build/midterm-switch`，输出文件名仍为`rt1064.axf`；工程名与工具链产物名分别管理。

生产固件身份仍按C31版本和实际产物SHA256辨识，不能仅从新工程名推定板上固件已更新。MPC是本次采用的工程名称；此前Phase 2标定缺口、Phase 3完成范围以及规划器尚未接入主控制路径的状态，继续以[阶段交付](c31-speed-planning-20261105/README.md)为准。

## 验证与证据

1. 对比更名前工程备份与新文件，字节及SHA256完全一致；6个Python路径消费者通过语法解析。
2. 仓库现有Keil工程共4162条源码引用均指向仓库内实际文件。活动工具/测试/CI入口中无旧主工程路径残留。
3. `static-safety`运行`20261106T103644-267678a078`，environment与static-safety两个节点PASS。
4. 从新路径执行实际主工程隔离Keil纯构建，0错误、0警告；另在私有工作区验证既有`-Variant switch`生成/构建入口。

可携带记录见[MPC_20261106重命名证据](2026-11-06_MPC_PROJECT_RENAME.json)，本地原始备份/构建日志位于`.build/project-rename-mpc-20261106`。本轮仅命名与路径调整，没有更改控制源码、标定数据或算法参数，未重复执行无代码变化的完整Host回归；之前3项既存失败和数据集待接入项不因改名而关闭。

本次读取使用原生Markdown/源码、XML与JSON结构解析（`structured_parsed`），字节对比、哈希、引用解析与构建为`computed`；无仅依赖`md_extracted`的精确数值，也无新增`pdf_verified`或物理实测结论。未操作串口、烧录、复位、提交或push；全局配置、技能与持久Agent行为未改。
