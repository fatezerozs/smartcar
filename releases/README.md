# 本次固件

`MIDTERM_C26_19/rt1064.axf` 是新候选，`MIDTERM_C26_17/rt1064.axf` 是保留的旧实车版本。分别核对各目录 `SHA256SUMS.txt` 和 `release.json`。AXF 是烧录/调试产物，不是 Keil 工程入口。

完整克隆仓库后打开 `demo/mdk/midterm_switch.uvprojx`，选择 `nor_sdram_zf_dtcm`，编译后由操作者使用自己的下载器设置烧录。也可按 `docs/TEAM_HANDOFF_20261022.md` 重新生成该工程。C26 OFF 烧录和搬车；ON 在连续看到有效跑道后自动启动，OFF 停车。

C19 仅完成软件和合成场景验证，尚未实车验收完整环岛或整圈。C17 有真实无线/视频运行证据，也未完成完整赛道验收。当前源码标识为 C19，不能用当前源码重编译声称复现原 C17 二进制。
