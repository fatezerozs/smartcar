# NEWNEW全版本分类目录

更新：2026-11-13。范围：NEWNEW V1～V11及其后续；**此前C26、WC、RINGLOG不在本次上传范围**。原云仓既有历史内容不删除、不移动。

NEWNEW当前最新为队友归档的[V11](NEWNEW_C26_DIRECT_20261112_V11/README.md)：直道请求1.5m/s、写入校验通过，最近重烧后赛道待验证。V10及以前均作为历史对照保留，不能把归档当作升级、回退或新实车验收。

| 版本 | 分类/主要变化 | 源码 | 固件 | 状态 |
| --- | --- | --- | --- | --- |
| [V1](NEWNEW_C26_DIRECT_20261111_V1/ARCHIVE_20261113.md) | 参考工程直接适配与C26发车 | [源码ZIP](NEWNEW_C26_DIRECT_20261111_V1/source-code.zip) | [AXF](NEWNEW_C26_DIRECT_20261111_V1/rt1064.axf) | 原范围保留，本次未复测 |
| [V2](NEWNEW_C26_DIRECT_20261112_V2/ARCHIVE_20261113.md) | 1.7m/s速度候选 | [源码ZIP](NEWNEW_C26_DIRECT_20261112_V2/source-code.zip) | [AXF](NEWNEW_C26_DIRECT_20261112_V2/rt1064.axf) | 原范围保留，本次未复测 |
| [V3](NEWNEW_C26_DIRECT_20261112_V3/ARCHIVE_20261113.md) | 1.0m/s速度候选 | [源码ZIP](NEWNEW_C26_DIRECT_20261112_V3/source-code.zip) | [AXF](NEWNEW_C26_DIRECT_20261112_V3/rt1064.axf) | 原范围保留，本次未复测 |
| [V4](NEWNEW_C26_DIRECT_20261112_V4/ARCHIVE_20261113.md) | S弯舵量和快速换向保护基线 | [源码ZIP](NEWNEW_C26_DIRECT_20261112_V4/source-code.zip) | [AXF](NEWNEW_C26_DIRECT_20261112_V4/rt1064.axf) | 兼容保护基线；非全赛道验收 |
| [V5](NEWNEW_C26_DIRECT_20261112_V5/ARCHIVE_20261113.md) | 阈值/避砖尝试；曾发生入环回归 | [源码ZIP](NEWNEW_C26_DIRECT_20261112_V5/source-code.zip) | [AXF](NEWNEW_C26_DIRECT_20261112_V5/rt1064.axf) | 入环回归历史，勿默认当基线 |
| [V6](NEWNEW_C26_DIRECT_20261112_V6/ARCHIVE_20261113.md) | 恢复V4环岛核心并隔离避砖mask | [源码ZIP](NEWNEW_C26_DIRECT_20261112_V6/source-code.zip) | [AXF](NEWNEW_C26_DIRECT_20261112_V6/rt1064.axf) | 原范围保留，本次未复测 |
| [V7](NEWNEW_C26_DIRECT_20261112_V7/ARCHIVE_20261113.md) | 普通道路双边居中 | [源码ZIP](NEWNEW_C26_DIRECT_20261112_V7/source-code.zip) | [AXF](NEWNEW_C26_DIRECT_20261112_V7/rt1064.axf) | 原范围保留，本次未复测 |
| [V8](NEWNEW_C26_DIRECT_20261112_V8/ARCHIVE_20261113.md) | 单边居中与边距限速 | [源码ZIP](NEWNEW_C26_DIRECT_20261112_V8/source-code.zip) | [AXF](NEWNEW_C26_DIRECT_20261112_V8/rt1064.axf) | 原范围保留，本次未复测 |
| [V9](NEWNEW_C26_DIRECT_20261112_V9/ARCHIVE_20261113.md) | 环岛准备/出口目标和数组索引修正 | [源码ZIP](NEWNEW_C26_DIRECT_20261112_V9/source-code.zip) | [AXF](NEWNEW_C26_DIRECT_20261112_V9/rt1064.axf) | 原范围保留，本次未复测 |
| [V10](NEWNEW_C26_DIRECT_20261112_V10/ARCHIVE_20261113.md) | 进出环连续性、浅砖候选与1.4m/s直道目标 | [源码ZIP](NEWNEW_C26_DIRECT_20261112_V10/source-code.zip) | [AXF](NEWNEW_C26_DIRECT_20261112_V10/rt1064.axf) | 原范围保留，本次未复测 |
| [V11](NEWNEW_C26_DIRECT_20261112_V11/ARCHIVE_20261113.md) | 队友1.5m/s速度候选；以原交接为准 | [源码ZIP](NEWNEW_C26_DIRECT_20261112_V11/source-code.zip) | [AXF](NEWNEW_C26_DIRECT_20261112_V11/rt1064.axf) | 已写入校验；重烧后赛道待验 |

## 如何选择与恢复

- 下载指定版本的source-code.zip并解压，打开其版本根目录/demo/mdk/newnew_c26.uvprojx；目标nor_sdram_zf_dtcm。不要将不同版本的头文件、SDK、AXF或参数混用。
- 每版原始清单/原校验与新归档清单分别保留。源码压缩包可能因剔除记录而与原本地ZIP不同，但保留的源码成员逐字节一致。
- V1原归档没有release.json；本次新增的是归档身份，不冒充原构建清单。V10/V11原云端代码包和固件不覆盖。
- 仅标注程序速度请求；Phase2计数比例的适用误差与实际车速仍按原交接，不将数值当作新标定。
- 本次仅整理上传，没有重新构建、烧录、运行或新增通过结论。

[机器可读清单](newnew-catalog.json) · [本次归档说明](../docs/changes/2026-11-13_NEWNEW_VERSION_ARCHIVE.md)
