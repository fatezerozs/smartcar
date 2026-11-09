> **维护口径（2026-11-09）：** 本文独立SD工程的已验收事实与历史输入保持不变；Phase2最新诊断没有SD运动联合验收，也未更新本工程发布镜像。跨工程的当前进度与入口由统一维护总览区分。 统一状态与历史适用范围见[文档维护总览](../../docs/DOCUMENTATION_STATUS.md)。

# SD Performance 独立性能测试固件

原交付：2026-11-06；维护：2026-11-07。状态：独立实板的24.75MHz、49.5MHz两档16MiB写入与全量回读均已通过，见[性能实测报告](evidence/performance-hardware-20261105T163728Z/PERFORMANCE_REPORT.md)。后续5文件另有[断电后PC校验](evidence/readback-20261105T183635Z-9052/PERSISTENCE_REPORT.md)。

目的：测量现有 1-bit / 3.3V 接线上 24.75MHz 与标准 High Speed 49.5MHz 两档的文件写入性能。此前 185KiB/s 来自 CPU 24MHz、4KiB 底层传输且包含模式/CRC 处理的诊断工程，不能代表 SD 卡上限。

## 工程与原构建记录

- 工程：`mdk/sd_performance.uvprojx`，目标 `SD_Performance`。
- 最终 AXF：`artifacts/run-20261106T003457153-37440/Objects/sd_performance.axf`。
- 最终 BIN：`artifacts/run-20261106T003457153-37440/Objects/sd_performance.bin`，基址 0x70000000，39168 字节。
- MAP、构建日志与校验：同 run 目录下 `Listings/sd_performance.map`、`keil-build.log`、`verification.json`。

以上为原构建产物路径；构建阶段没有下载、烧录、复位或打开串口。后续用户已烧录并授权执行独立测试，实际结果见页首报告；不以这些本地产物路径自动授权再次烧录。旧版固件不具备BENCH25/BENCH50命令。
原 `demo/sd_diagnostic/` 中用户打开过的工程、源码、实板证据和全部旧产物未修改。

## 变更及边界

- CPU 24MHz -> 600MHz：采用仓库原板级配置中的 ARM PLL /2、VDD_SOC 1.275V、IPG /4 策略，不超频。开启指令缓存；数据缓存保持关闭，以维持 SD DMA 一致性。Flash 原位执行、数据和栈留在 OCRAM，不依赖 SDRAM。未改 USB PLL / FlexSPI2 时钟。
- SD 初始化仍采用 1-bit、3.3V、最高 25MHz，实际预计 24.75MHz。成功协商 `kSD_TimingSDR25HighSpeedMode` 后，才允许独立命令将主机时钟提高到最高 50MHz，现有 198MHz 根时钟下预计 49.5MHz。还检查电压模式、总线宽度和实际返回时钟；不支持则拒绝该档测试。
- SD High Speed 在这里指标准 3.3V 50MHz 模式。1.8V、UHS、DDR、4/8-bit 和 DATA3 检测仍禁用，不改 DATA2/3、VSELECT 或车辆外设接线。
- 底层 bounce buffer 扩为 64KiB，每次最多 128 扇区；FatFs 遇簇边界仍可拆成较小多扇区请求。
- 每档新建并预分配连续的 16MiB 文件；没有足够连续空间则报错，不整理/格式化磁盘。开启独立 FatFs 配置 `FF_USE_EXPAND=1`，原 SDK/FatFs 驱动文件未复制改写。
- 固定 256 次 64KiB 写入，随后同步、关闭、重开并全量回读校验。使用新的 `P0000000.BIN` 起名称，`FA_CREATE_NEW` 防覆盖；两档合计最多 32MiB，另有 BASIC 64KiB，均保留。
- 载荷含块序号、tag、确定性模式及表驱动 IEEE CRC32。生成/CRC 位于单次 f_write 计时之外，但仍计入完整写入+同步耗时。

## 测试顺序

COM14 为有线，UART1 B12/B13，115200 8N1、无流控。启动应看到 `SDPERF v1 CPU600MHz`；HELP 应带 `SDPERF v1`。

```text
SDDIAG HELP
SDDIAG STATUS
SDDIAG RUN BASIC
SDDIAG RUN BENCH25
SDDIAG RUN BENCH50
SDDIAG PERF RESULT
```

BENCH25 必须在本次 BASIC 成功后启动；BENCH50 必须在 BASIC 与 BENCH25 均成功后启动。一次 BENCH50 会消耗前置成功状态，再测须重跑 BASIC/BENCH25。测试不随上电自动启动。
卡不支持 High Speed 或校验失败，不继续提速；部分写入/已预分配文件也保留。预分配文件即使测试失败也可能显示完整大小，应以 RESULT 的 written/read/CRC 结果判断，不按文件大小判断成功。

已准备 `test_performance.ps1`，用于换烧后采集同样的有界测试顺序；它先识别固件标识和 cpu_hz=600000000，再允许写入，BENCH25 失败则不发送 BENCH50。脚本无烧录/复位功能，本次仅检查语法，没有执行。

## 指标解释

`PERF RESULT` 的所有吞吐单位为 KiB/s，时间为微秒（total_ms 除外）：

- `write_io_KiBps`：总字节 / 各次 f_write 调用耗时之和；包含 FatFs、驱动及 bounce 拷贝，不含块生成/CRC、预分配和最后同步。可用于观察文件 I/O 管线的最佳吞吐。
- `write_sync_KiBps`：从第一块处理开始到最终 f_sync/f_close 完成的平均吞吐，含块生成/CRC 和最终落盘，适合作为实际记录能力的主要指标。
- `read_io_KiBps` 与 `read_verified_KiBps`：分别为单独读取 I/O 和包含全量 CRC/模式验证的吞吐。
- `prealloc_us`：f_expand + 预分配后的 f_sync；预分配成本未计入 write_sync，但计入 total_ms。首次建文件、空间查询等也计入 total_ms。
- `max_us/p95_us/p99_us`：64KiB f_write 的最大/最近秩延迟，不能直接与旧版 4KiB 延迟逐项对比；末尾同步延迟另列 `sync_us`。
- 计时基于 1ms SysTick 与当前 VAL 的微秒插值，不直接使用 600MHz 下约 7 秒就回卷的 DWT CYCCNT 作为全程计时器。
- 49.5MHz 单线的原始比特率算术上限为 6.1875MB/s（约 5.90MiB/s），24.75MHz 为 3.09375MB/s。协议、卡内写入、FAT 与软件开销会使实测更低；这些是计算上界，不是已测速度。

16MiB 属于短时有界测试，不能证明长时间 NAND 稳态或卡全容量性能。原 SDK 的部分硬件轮询无完整墙钟总超时的限制仍存在。错误结果中的未完成阶段统计不用于速率结论。

## 构建与验证

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\shengchanli\smartCar\smartcar\demo\sd_performance\build.ps1
```

每次使用独立 run 输出，不覆盖旧 AXF/BIN。Keil ARMCLANG 6.21：零错误零警告；17 个对象严格匹配 MAP 白名单；两个 64KiB 缓冲和卡/host/描述符均位于片内 OCRAM，64 字节对齐。Code 26580、RO 4880、RW 16、ZI 192804 字节（含保留栈/堆），Flash 镜像结束低于保留配置页。

主机测试：`evidence/host-core.log`（16 项）、`evidence/host-bench.log`（16 项），GCC -O2 -Wall -Wextra -Werror。覆盖原诊断故障、命令门控、预分配失败、拒绝调时钟、空间不足、名称冲突、短写/短读、CRC、大小、同步/关闭/重开错误、有界次数、计时/百分位和结果输出完整性。寄存器级时钟/电压及真实 FatFs 连续簇分配未被替身模拟验证。
证据层：XML/JSON为structured_parsed；编译、主机执行、MAP/内存与哈希检查及比特率换算为computed。以上构建记录本身不包含实板测量；后续实板性能及PC回读结果独立链接于页首，无新增md_extracted/pdf_verified结论。

变更仅在新增 `demo/sd_performance/`。基线 1219 文件（含原 C31、Recorder、既有 SD Diagnostic）复核 0 变化；没有修改全局配置、技能或持久代理行为。详见 `evidence/preservation-final.json`。
