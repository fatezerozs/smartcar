# 版本固件与校验

当前源码为 **MIDTERM_C26_31**。当前能力/待办见 [演进档案](../docs/PROJECT_EVOLUTION.md)，本次变更见 [2026-10-23 提交说明](../docs/changes/2026-10-23_C31.md)。

工程入口为 `demo/mdk/midterm_switch.uvprojx`，目标 `nor_sdram_zf_dtcm`。
C31 仍采用 C26 启停，上电识别赛道若干秒后启动尚未实现。
发布副本有明确版本；重新编译当前源码不能冒称复现旧版本。

## 已归档版本

| 版本 | 固件 | 清单 | 校验 |
| --- | --- | --- | --- |
| MIDTERM_C26_31 | [AXF](MIDTERM_C26_31/rt1064.axf) | [release.json](MIDTERM_C26_31/release.json) | [SHA256](MIDTERM_C26_31/SHA256SUMS.txt) |
| MIDTERM_C26_30 | [AXF](MIDTERM_C26_30/rt1064.axf) | [release.json](MIDTERM_C26_30/release.json) | [SHA256](MIDTERM_C26_30/SHA256SUMS.txt) |
| MIDTERM_C26_29 | [AXF](MIDTERM_C26_29/rt1064.axf) | [release.json](MIDTERM_C26_29/release.json) | [SHA256](MIDTERM_C26_29/SHA256SUMS.txt) |
| MIDTERM_C26_28 | [AXF](MIDTERM_C26_28/rt1064.axf) | [release.json](MIDTERM_C26_28/release.json) | [SHA256](MIDTERM_C26_28/SHA256SUMS.txt) |
| MIDTERM_C26_27 | [AXF](MIDTERM_C26_27/rt1064.axf) | [release.json](MIDTERM_C26_27/release.json) | [SHA256](MIDTERM_C26_27/SHA256SUMS.txt) |
| MIDTERM_C26_26 | [AXF](MIDTERM_C26_26/rt1064.axf) | [release.json](MIDTERM_C26_26/release.json) | [SHA256](MIDTERM_C26_26/SHA256SUMS.txt) |
| MIDTERM_C26_25 | [AXF](MIDTERM_C26_25/rt1064.axf) | [release.json](MIDTERM_C26_25/release.json) | [SHA256](MIDTERM_C26_25/SHA256SUMS.txt) |
| MIDTERM_C26_24 | [AXF](MIDTERM_C26_24/rt1064.axf) | [release.json](MIDTERM_C26_24/release.json) | [SHA256](MIDTERM_C26_24/SHA256SUMS.txt) |
| MIDTERM_C26_23 | [AXF](MIDTERM_C26_23/rt1064.axf) | [release.json](MIDTERM_C26_23/release.json) | [SHA256](MIDTERM_C26_23/SHA256SUMS.txt) |
| MIDTERM_C26_19 | [AXF](MIDTERM_C26_19/rt1064.axf) | [release.json](MIDTERM_C26_19/release.json) | [SHA256](MIDTERM_C26_19/SHA256SUMS.txt) |
| MIDTERM_C26_17 | [AXF](MIDTERM_C26_17/rt1064.axf) | [release.json](MIDTERM_C26_17/release.json) | [SHA256](MIDTERM_C26_17/SHA256SUMS.txt) |

产物及校验只用于确定版本，不自动证明入环、出环、不出界、速度或完整赛道验收。
C31 的软件回归和实拍缩略图回放不能替代真实自动驾驶。使用各目录清单核对下载文件。
临时构建输出、原始录制及个人 IDE 状态不在发布列表内。
