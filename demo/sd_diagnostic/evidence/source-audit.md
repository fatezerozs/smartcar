# 源码审计

读取方式：Git 跟踪清单定位，限定 Recorder 源码与明确依赖文件；C/头文件原文审计，Keil XML 原生解析。没有读取 PDF、Word 或长篇版式文档，没有 md_extracted 证据。

调用链：

`UART1 main -> diag_command -> f_mount/f_getfree/f_open/f_write/f_sync/f_close/f_read -> disk_initialize/status/read/write/ioctl (board.c) -> SD_Init/SD_WriteBlocks/SD_ReadBlocks/SD_PollingCardStatusBusy (原 fsl_sd.c) -> SDMMCHOST_* (原 non_blocking/fsl_sdmmc_host.c) -> 原 fsl_usdhc.c -> USDHC1`。

原 Recorder 入口的 recorder_disk.c / recorder_sd_pins.c / recorder_sd_osa.c 已审计，发现依赖 FreeRTOS、生产头文件及 SDRAM；本工程未链接这些文件，改为独立 diskio/板级/单线程 OSA 胶合代码，复用原 SDK 存储驱动。

`sd_policy.c` 在先解析 SDK 类型后，将实现引用的 kSD_Support4BitWidthFlag 局部定义为 0，避免原 SDK 根据卡能力无条件切换 4-bit。`host_policy.c` 对 1.8V/1.2V、4/8-bit、UHS/DDR、高速与 DATA3 检测能力做同样的编译范围限制，避免 host->capability 的无条件赋值误报能力。卡用户参数另设不支持 IO 电压切换和 maxFreq=25MHz。原驱动文件字节保持原状。

引脚依据原 sdmmc_config.h、Recorder 引脚代码与 SDK IOMUXC 定义：D4=GPIO3_IO04 低有效检测；B23=GPIO1_IO23 低有效 SD 电源；SD_B0_00 CMD、SD_B0_01 CLK、SD_B0_02 DATA0。仅复用这三根总线线，无 DATA2/3、VSELECT 或 PWM 初始化；UART1 使用 AD_B0_12/13（B12/B13）。不初始化无线 UART8。

不复用原 system_MIMXRT1064.c：其 ARMCLANG 分支会复制向量到 0x80000000 SDRAM。独立 diag_system.c 只设置 FPU、看门狗、SysTick 和 Flash VTOR；自有 scatter 只使用 Flash/OCRAM。CPU 切至 OSC 24MHz，再初始化 SYS PLL/PFD0 与 USDHC1/UART 所需根时钟；不调用全板时钟/引脚、摄像头或车辆初始化函数。

MAP 对象严格允许列表与 DMA 对象地址、初始 SP/Reset 向量、USDHC1 驱动中断桥接、SysTick 强实现由 verify.py 检查。启动文件包含各外设弱默认 IRQ 名称，不代表链接了对应业务。

发现并保留的边界：原 SD_PollingCardStatusBusy 在非 busy 但 CMD13 状态持续非 idle 时不递减计数；原 USDHC 某些寄存器轮询也不设墙钟总超时。此任务优先原样复用生产驱动，没有借此改写生产文件。容量有限与所有异常有界返回是不同性质，使用说明已明确区分。
