# C28 夜间调试归档：2026-11-04 至 11-05

**结论：末轮 WC7 实车入环仍失败，未完成环岛验收。** 此处不是 main/C32 的替换版本。

- [完整总结、版本变化、测试及失败记录](../../docs/changes/2026-11-05_C28_NIGHT_DEBUG.md)
- [完整工程 ZIP：WC7 源码与原始 WC2 恢复](C28_WC7_source_and_WC2_recovery.zip)、[校验](SHA256SUMS.txt)
- [关键代码与测试审阅副本](review-code/)：不可单独编译；完整依赖在 ZIP 中。
- [逐轮历史记录](records/)、[结构化测试证据](../../evidence/2026-11-05_c28_night/summary.json)

ZIP 内 `START_HERE.md` 区分两种用途：

1. 原始 WC2 固件恢复：`demo/mdk/WC2_original_download.uvprojx`，仅用 Flash → Download，不 Build/Rebuild。
2. WC7 实验源码构建：`demo/mdk/midterm_switch.uvprojx`，不是已通过实车入环的版本。

两者目标均为 `nor_sdram_zf_dtcm`。保留 C26 启停、当前摄像头位置和原始 WC2 校验；没有提供未重建的 WC2 源码等价承诺。个人 IDE 设置、下载器身份及原始视频/串口不在公共归档中。
