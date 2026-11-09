> **维护口径（2026-11-09）：** 本文独立SD工程的已验收事实与历史输入保持不变；Phase2最新诊断没有SD运动联合验收，也未更新本工程发布镜像。跨工程的当前进度与入口由统一维护总览区分。 统一状态与历史适用范围见[文档维护总览](../../docs/DOCUMENTATION_STATUS.md)。

# 当前 SD Performance 固件增加只读导出

2026-11-06。按用户要求，直接扩展原 `SD_Performance` Keil 工程，没有新建独立固件；已有 BASIC/STRESS/BENCH25/BENCH50 功能保留。

## 当前状态与下一步

用户已确认核心板断电再上电。已完成导出功能、PC 下载器、独立校验器、主机故障测试和 Keil 构建；尚未换烧此新版本，也未开始下载或核验实际断电后的文件。

**请用原工程 `mdk/sd_performance.uvprojx` 换烧并运行本次新 AXF：**

`artifacts/run-20261106T005352591-44312/Objects/sd_performance.axf`

BIN 同目录 `sd_performance.bin`，基址 0x70000000，42816 字节。构建日志、MAP、SHA256 和内存检查在同 run 目录。ARMCLANG 6.21：0 Error(s), 0 Warning(s)。上电默认仍是 UART1 / COM14、115200 8N1；应看到 `SDPERF v2 EXPORT1 CPU600MHz`。

**换烧后不要执行 BASIC/STRESS/BENCH 写入命令，也不要格式化或删文件。** 本次目的是读取并验证断电前的原有文件。下载器不会发送任何 RUN 命令。代理本阶段未执行烧录、复位或串口通信。

## 导出协议

```text
SDDIAG EXPORT INFO P0000000.BIN
SDDIAG EXPORT READ P0000000.BIN 0 32768
SDDIAG EXPORT BAUD 921600
```

白名单仅为已有 `S0000000.BIN`、`S0000001.BIN`、`S0000002.BIN`、`P0000000.BIN`、`P0000001.BIN`。拒绝目录、任意路径和其他文件。文件打开标志固定 FA_READ；模块仅挂载、打开、定位、读、关闭，不调用写入、预分配、删除、格式化或分区 API。MAP 引用审计检查该模块没有指向这些写 API 的直接调用；整个固件仍保留原来需要用户显式触发的写入测试。

INFO 返回文件大小。READ 每次最多 32768 字节，先读完并关闭文件，然后返回文本头 `SDREAD DATA name=... offset=... length=... crc32=...`、精确长度的二进制原文、尾标记 `SDREAD END`。PC 同时检查文件名/偏移/长度、传输 CRC32 和帧尾；CRC 错误最多重取同一块三次，不写卡。读取/关闭失败不发送数据帧。

BAUD 仅接受 115200、460800、921600。ACK 在旧速率发送，等待 TX 完成和 20ms 再切换；复位后恢复 115200。高速能否通过当前 USB-UART 链路还需实测；失败停止，保留本机 .partial 文件，不自动复位。可在人工重启后用较低波特率重试。

## PC 下载与独立校验

换烧后可以由代理继续执行，或手动执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\shengchanli\smartCar\smartcar\demo\sd_performance\download_existing.ps1
```

默认 COM14，起始连接 115200，识别 `SDPERF v2 EXPORT1` 后切换到 921600 并再次握手，再逐一只读下载 5 个已知文件。总数据48.125MiB；921600 下理论纯线时间约 9 分钟，实际加上读卡、CRC 和请求开销会更长。115200 则明显更慢。
脚本本次只以 `-CheckOnly` 做过离线编译/自检，未连接实际串口。下载前不会改动 SD 时钟、创建测试文件或重跑测试。连接 DTR/RTS 禁用；成功后恢复 115200 并关闭端口，失败也关闭端口。

下载存入新 `evidence/readback-时间-PID/`，采用 CreateNew，不覆盖本机已有文件。文件先为 `.partial`，完整收取后改名；记录原始协议文本日志。收取后 Python `struct` / 标准库 `zlib.crc32` 独立检查：

- S 文件：4096 字节块，魔数、全程递增序号、固定 tag、原数据模式、每块 CRC32；预期大小分别 64KiB、16MiB、64KiB。
- P 文件：65536 字节块，魔数、递增序号、固定 tag、原性能测试数据模式及每块 CRC32；各 16MiB。
- 所有文件计算完整 SHA256，检查长度及检查期间的文件稳定性；结果写入 `pc-verification.json`，采集状态写入 `acquisition.json`。

只有所有文件实际下载并通过 PC 校验，才能根据用户确认的断电过程给出本轮断电后数据保留的结论。当前 `persistence_verified=false`，不能拿主机替身通过代替实板文件验证。
串口导出仍通过核心板 USDHC/FatFs 读取，PC 校验算法独立；不等同 PC 读卡器独立解析分区/FAT，也不证明任意异常掉电时都安全。

## 验证证据和限制

- `evidence/host-export.log`：FA_READ、白名单、帧格式/已知 CRC、挂载/打开/定位/短读/关闭/读 API 失败、越界/尾随输入、波特率允许列表和切换错误。
- `evidence/host-core-export.log`：原诊断及门控 16 项回归通过。
- `evidence/host-bench-export-regression.log`：原性能核心 16 项回归通过。
- `evidence/host-pc-checker.log`：9 个 PC 校验用例通过，包括改内容后重新计算合法 CRC 仍能由模式/序号/tag 检查发现，以及截断/追加块；校验器未改动夹具。
- `evidence/host-download-client.log`：C# 下载器离线编译、CRC32 标准向量和帧身份检查通过，没有打开 COM。
- 本次 MAP 严格允许 18 个项目对象，export_buffer 为 32768 字节、64 字节对齐且在 OCRAM；未引入车辆/视觉/RTOS 业务对象。Code 30016、RO 5072、RW 16、ZI 226148 字节（含保留栈/堆）。

新高速串口、电源断开后真实文件内容与完整大文件下载仍未验证；原 SDK 部分寄存器轮询的总超时限制仍保留。
证据层：Keil XML/JSON 为 structured_parsed，编译/单元执行/MAP/哈希为 computed；无 md_extracted/pdf_verified。用户断电确认是操作记录，不是 PC 文件校验结果。

## 文件改动与自审

修改：`board.c`, `diag_core.c`, `include/diag.h`, `mdk/sd_performance.uvprojx`, `mdk/sd_performance.uvoptx`, `configure.py`, `verify.py`, `tests/test_core.c`。
新增源码/工具/说明：`export.c`, `download_existing.ps1`, `tools/SerialExport.cs`, `tools/verify_download.py`, `tests/test_export.c`, `tests/test_verify_download.py`, `EXPORT.md`；新增构建、测试日志、恢复审计和下载夹具均在本工程 evidence/artifacts/tests 中。
原修改前快照：`evidence/export-preflight-20261105T164657Z/before/`；Git 状态和文件哈希与用户断电确认均保留。

首次构建受到 Keil `.uvoptx` 缓存 ListingPath 影响，MAP 写入了旧 run 目录。现已修正 configure.py 同步更新项目和用户选项中的输出路径，并用原对象文件重链接恢复旧 MAP；恢复后的 SHA256 与原记录完全一致。旧 AXF、BIN、构建日志未改变，旧 MAP 也已恢复精确哈希。审计见 preflight 的 `map-recovery/result.json`，异常尝试的 MAP 副本保留。

自审确认：只在原 SD Performance 工程增加功能；导出文件以只读打开；下载器不发送测试写入命令；文件与本机输出均防覆盖；旧性能构建证据已完整复核；原 C31/Recorder/SD Diagnostic 的 1219 项基线仍零变化；未修改全局配置、技能或持久代理行为。
