# NEWNEW C26 V10固定交付

- [完整工程ZIP](source-code.zip)：包含SDK、启动文件、Keil工程及测试。解压后打开`NEWNEW_C26_DIRECT_20261112_V10/demo/mdk/newnew_c26.uvprojx`，目标`nor_sdram_zf_dtcm`。
- [固定AXF](rt1064.axf) · [链接MAP](rt1064.map) · [文件校验](SHA256SUMS.txt)。
- [发布身份](release.json) · [源码逐文件清单](source-manifest.json)。
- [交接与未完成项](../../docs/changes/2026-11-12_NEWNEW_C26_V10_HANDOFF.md)。

AXF SHA256：`a7151e034743ee363035e6283cfe0c28924e565fb901178e627722a8d57d6e78`。Keil0错误0警告，所列Host通过；V10实车待验。用户手动烧录，C26启停；直道目标1.4m/s，弯道降速。

源码包按仓库规则剔除原始录制、临时产物和个人IDE状态，与原电脑含录制的历史ZIP不同；所有编译输入与原V10一致。来源为操作者提供的newnew.zip及本轮修改，原SDK和文件头保留；需要本机Keil/设备包，Host检查另需MSVC/ASAN。
