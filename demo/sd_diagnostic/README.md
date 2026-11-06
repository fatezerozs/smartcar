# SD Diagnostic 使用说明

独立 Keil / ARMCLANG 工程：`mdk/sd_diagnostic.uvprojx`，目标 `SD_Diagnostic`。
工程只引用本目录源码与现有 SDK/FatFs；无 Recorder、C31 或车辆业务源码依赖。

维护：2026-11-07。独立实板的卡初始化、FAT32、64KiB BASIC和16MiB STRESS已通过，[实板报告](evidence/hardware-20261105T162039Z/HARDWARE_REPORT.md)保留具体配置与观测范围。后续5个文件另有[断电后PC回读校验](../sd_performance/evidence/readback-20261105T183635Z-9052/PERSISTENCE_REPORT.md)。这些结果不覆盖RecorderTrain运动并行或任意写入时刻掉电安全。

## 当前交付

已独立构建，0 Error(s), 0 Warning(s)。最终构建目录：`artifacts/run-20261106T001438910-29796/`。
其中 `Objects/sd_diagnostic.axf`、`Objects/sd_diagnostic.bin`、`Listings/sd_diagnostic.map`、`keil-build.log`、`verification.json` 为同次构建证据。
BIN 基地址为 **0x70000000**，长度 36032 字节；本说明不构成烧录授权。
上述构建阶段本身未烧录、复位或打开串口；后续独立实板验证已执行，见页首报告。原报告未通过调试器逐字节读回Flash，镜像身份限制仍保留。

## 重新构建（只构建）

在 PowerShell 执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File C:\shengchanli\smartCar\smartcar\demo\sd_diagnostic\build.ps1
```

默认工具路径为 `E:\ElecTronic\Keil\UV4\UV4.exe`，可用 `-Uv4` 指定。
脚本锁定本工程构建，创建独立 run 目录，重新配置本工程输出，调用 UV4 `-b`，检查零错误/零警告，使用 fromelf 转 BIN，再执行 MAP/向量/原文件保护校验。
不带下载命令，没有编译前后执行钩子；调试前自动下载已关闭。
不要直接在已有输出目录上重复构建，以免覆盖已有 AXF；始终使用此脚本分配新 run 目录。
历史失败日志和中间构建保留以便追溯，以 `evidence/latest-build.txt` 指向的验证成功目录为准。
项目依赖仓库内原 SDK 路径，不是脱离仓库的自包含 SDK 包。

## 再次板端测试的操作规程

人工确认诊断固件替换当前运行镜像的范围，保存当前生产固件并核对 AXF/BIN 哈希；避免整片擦除及触及末尾配置页。
历史板端步骤已经执行；再次测试须按当前授权范围区分烧录/复位与串口读写，不能把本文当成自动重跑指令。
串口为 **UART1，B12 TX / B13 RX，115200、8N1，无流控**。COM14 为有线、COM10 为无线（用户确认）；固件只初始化 UART1，不能据此推断无线 UART8 可用。
历史实测使用COM14到UART1；再次测试先核对当前端口和接线。启动后只输出ready，不自动初始化SD或创建文件。
依次手动输入：

```text
SDDIAG HELP
SDDIAG STATUS
SDDIAG RUN BASIC
SDDIAG RESULT
SDDIAG RUN STRESS
SDDIAG RESULT
```

- STATUS 尝试初始化/挂载已有文件系统，报告卡检测、初始化返回码、容量/扇区、实际 USDHC 总线宽度、协商时钟、配置根时钟、3.3V 策略及剩余空间。它不创建测试文件、不格式化。
- BASIC 创建 `2:/S0000000.BIN` 起的空闲 8.3 文件名。只用 `FA_CREATE_NEW`，存在则递增，最多探测 1000 万个名称，不覆盖已有文件。
- BASIC 写入 **64KiB**，每块 **4096 字节**：LE32 魔数 `0x53444447`、LE32 递增块序号、LE32 本次 tag、确定性数据模式、末尾 LE32 CRC32。CRC 覆盖块前 4092 字节，多项式为反射 IEEE `0xEDB88320`，初始/最终异或 `0xFFFFFFFF`。
- 写后 `f_sync`、关闭、重新打开，检查文件大小、序号/tag/数据模式/CRC，然后关闭。所有文件（含失败的部分文件）保留。
- STRESS 必须在同次启动 BASIC 成功后显式输入，单次固定 **16MiB**、4096 次 4KiB 写入并全量回读；不会循环续写。压力测试后须重跑 BASIC 才能再测。
- 两种测试都预留 64KiB 空间余量，遇短写、短读、CRC/大小错误、FatFs 错误立即终止本次测试；底层 SD 传输错误锁定后续磁盘访问。
- RESULT 给出本次文件名、失败阶段、错误次数、累计写入/读取字节和时间。`errors` 统计本次失败及清理关闭失败，不是全寿命错误计数。
- `write_KiBps/read_KiBps` 是应用阶段平均吞吐（包括数据生成/CRC 校验开销），不是 SD 总线极限。`max_ms/p95_ms/p99_ms` 针对每次 4KiB `f_write`，最近秩百分位；不含 sync/close，后者单列 `sync_close_ms`。计时粒度 1ms，短调用可显示 0ms。失败结果中未完成阶段的吞吐/耗时不应用于性能评价。

## 必须保留的限制

CPU 采用 24MHz 振荡器，USDHC1 根时钟来自 SYS PLL PFD0 /2，目标 198MHz；卡总线 1-bit、3.3V、最高 25MHz（实际分频值以 STATUS 为准）。这是保守独立诊断时钟策略，不能与生产配置下吞吐直接比较。
Flash 原位执行，向量位于 0x70002000；数据、SD 卡对象、DMA 描述符和 bounce buffer 位于片内 OCRAM，DCache 关闭，不依赖 SDRAM/DCD。
驱动复用通过独立编译单元 include 原 SDK C 文件并应用局部预处理策略；未复制或修改原驱动。
没有热拔插恢复保证；卡片异常后应保留日志，在后续授权范围内人工重启测试，不自动重初始化。
原 SDK 的部分硬件轮询仍没有完整墙钟总超时（例如 SD busy 状态非 idle 的分支、USDHC 时钟稳定等待）。本固件限制写入容量，并把 OSA 单次事件等待上限设为 1s，但不声称所有故障均能有界返回。主机模拟不覆盖这些寄存器级故障。
压力测试会在新文件中写入数据并更新 FAT 元数据；虽然不覆盖已有文件，断电/硬件故障仍可能影响同一文件系统。实际测试数据和供电条件由下一阶段人工确认。

完整验收与证据边界见 `ACCEPTANCE.md`；主机测试见 `tests/README.md`。
