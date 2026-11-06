# 当前基线构建清单（不是新构建验收）

日期：2026-11-05。本轮没有执行 Keil 新构建或链接，没有覆盖既有 AXF/BIN/MAP。

## 构建身份

- HEAD：`de1415743b004160f520d385c9e3e63008f52007`；dirty=true，包含既有已跟踪及未跟踪输入。
- 既有构建记录时间：`2026-11-05T10:00:44.8251706+08:00`。
- 工程：`demo/mdk/training/recorder_train.uvprojx`。
- target：`nor_sdram_zf_dtcm`；输出名称：`recorder_train`。
- 工程设备：`MIMXRT1064DVL6B`；Cortex-M7；SDK CPU 宏为 `CPU_MIMXRT1064DVL6A`。本地 SDK 的 fsl_device_registers.h 使用该 A 宏选择 MIMXRT1064.h；本轮不擅自改为 SDK 未列出的 B 宏。
- 配置工具链：`6210000::V6.21::ARMCLANG`。本轮实际查询的 armclang 为 Arm Compiler for Embedded 6.21，MDK Plus 5.39；证据 `armclang-version.txt`。
- 输入树 SHA-256：`9a87e7b4bfc86d78732963e02bc5d1c9d097a97dfd7b77f70c9b803603a2402e`；已记录输入项与当前字节匹配，不等于所有工具/选项都已纳入树哈希。

| 当前产物 | bytes | SHA-256（computed） |
|---|---:|---|
| `.build/recorder-train/Objects/recorder_train.axf` | 1406856 | `8fcd8c7ea83a8f9eb69614baca8987c80404118272149ad572b45aface28fe61` |
| `.build/recorder-train/Objects/recorder_train.bin` | 115392 | `cb0936ec72accf7eed0e73b88c78e598230ba29f793a279600da6007bfd118ed` |
| `.build/recorder-train/Listings/recorder_train.map` | 2243253 | `cfe6e7a3ce290669cd9a085d28fb29c9a966e92a2d5a63561481254804ef32e2` |

## C 编译与链接配置

Cads 原始字段：Optim=`7`、v6Lang=`3`、uC99=`1`、vShortEn=`1`、vShortWch=`1`、v6Lto=`0`。完整 XML 存在 `keil-project.json`，不把 GUI 字段编码冒充已经捕获的历史命令行。

MiscControls：

```text
-fno-common  -fdata-sections  -ffreestanding  -fno-builtin  -mthumb -Wno-invalid-source-encoding -fno-finite-math-only -include ../../recorder/include/recorder_build.h
```

全局 Define（完整值）：

```text
RECORDER_TRAINING=1,VISION_MIDTERM_TRAINING=1,VISION_ENCODER_ENABLE=1,VISION_MIDTERM_AUTONOMOUS=1,VISION_MIDTERM_ROUTE_TRIAL=1,VISION_ROUTE_SWITCH=1,VISION_START_SWITCH_ENABLE=0,MIDTERM_VISUAL_REVIEWED=1,CPU_MIMXRT1064DVL6A,SKIP_SYSCLK_INIT,XIP_EXTERNAL_FLASH=1, XIP_BOOT_HEADER_DCD_ENABLE =1, XIP_BOOT_HEADER_ENABLE =1,PRINTF_FLOAT_ENABLE=1, SCANF_FLOAT_ENABLE=1, PRINTF_ADVANCED_ENABLE=1, SCANF_ADVANCED_ENABLE=1, FSL_DRIVER_TRANSFER_DOUBLE_WEAK_IRQ=0, USB_STACK_BM, DEBUG, MCUXPRESSO_SDK, SD_ENABLED
```

有序 IncludePath：

1. `..\..\recorder\include`
2. `..\..\..\.build\recorder-train\adapted`
3. `..\..\..\libraries\third_party\FreeRTOS-Kernel\include`
4. `..\..\..\libraries\third_party\FreeRTOS-Kernel\portable\GCC\ARM_CM7\r0p1`
5. `..\..\user\inc`
6. `..\..\code`
7. `..\..\..\libraries\sdk\startup\mdk`
8. `..\..\..\libraries\sdk\deceive`
9. `..\..\..\libraries\sdk\drives`
10. `..\..\..\libraries\sdk\xip`
11. `..\..\..\libraries\sdk\utilities`
12. `..\..\..\libraries\sdk\utilities\debug_console`
13. `..\..\..\libraries\sdk\utilities\str`
14. `..\..\..\libraries\sdk\cmsis_drivers`
15. `..\..\..\libraries\sdk\CMSIS\Include`
16. `..\..\..\libraries\sdk\CMSIS\Driver\Include`
17. `..\..\..\libraries\sdk\components\lists`
18. `..\..\..\libraries\sdk\components\osa`
19. `..\..\..\libraries\sdk\components\serial_manager`
20. `..\..\..\libraries\sdk\components\uart`
21. `..\..\..\libraries\sdk\board`
22. `..\..\..\libraries\components\fatfs\source`
23. `..\..\..\libraries\components\fatfs\source\fsl_sd_disk`
24. `..\..\..\libraries\components\sdmmc`
25. `..\..\..\libraries\components\sdmmc\common`
26. `..\..\..\libraries\components\sdmmc\host\usdhc`
27. `..\..\..\libraries\components\sdmmc\osa`
28. `..\..\..\libraries\components\sdmmc\sd`
29. `..\..\..\libraries\components\usb\device`
30. `..\..\..\libraries\components\usb\include`
31. `..\..\..\libraries\components\usb\phy`
32. `..\..\..\libraries\components\usb\usb_cdc_adapter`
33. `..\..\..\libraries\zf_common`
34. `..\..\..\libraries\zf_driver`
35. `..\..\..\libraries\zf_device`

链接 scatter：`..\.\scf\MIMXRT1064xxxxx_flexspi_nor.scf`；额外链接选项：`--remove --entry=Reset_Handler`。既有 `recorder_train.lnp` 同时记录 microlib、remove、entry、map、callgraph 和输出路径，快照中保留原字节。

## 源文件与配置链

完整工程条目 390 项；当前启用的 C/汇编单元 193 项，清单见 `.build/c31-safety-20261105-phase01/active-compilation-units.json`。这些单元没有 GroupOption/FileOption 编译覆盖。最终保留符号仍受链接器删除未引用节影响。

- 控制器、actuator、speed、guard、startup、boot 来自 `demo/code/`。
- DRIVE 使用 `.build/recorder-train/adapted/vision_drive.c`；生成器只在该文件中将 route fast ticks 从 132 替换为 40，精确 diff 已保存。
- main、ISR 及 Capture/Vision/Control/Storage/Link 来自 recorder 专用入口/模块。
- camera、SD/FatFs 等私有适配源来自 adapted，来源锁与生成记录已备份。
- FreeRTOS port 来自 `portable/GCC/ARM_CM7/r0p1/port.c`，config 来自 recorder/include。
- 编译器强制包含 recorder_build.h；由 vision_profile.h 派生的 C31 表达式为 autonomous && route-switch && route-trial，当前三者均为 1。

运行配置区分宏默认值和初始化覆盖：启动资格 1000 ms、5 帧基础稳定资格、encoder signs=(-1,+1)、recorder max ticks=40/10ms、PI Kp=1.0、Ki=0.05；实际目标还受 steering/route target shaping 影响。profile duty cap=0.18 是目标请求域，最终正常 route PWM 物理域上限为 1000 permille。当前 max_run_ms 被覆盖为 0，不能依据默认宏 5000 ms 宣称总运行时限启用。

## 本轮分析命令与范围

`analysis-commands.json` 保存 4 个单元的完整 ARMCLANG 预处理命令与 fromelf 命令，均 exit=0。预处理使用工程 define/include/强制包含配置及项目现有 ARM check 的目标 ABI 参数；这不是新 Keil 构建，也不是历史实际编译器进程捕获。vision_control 无诊断，另外三个单元各有一个已存在的 include 文件名大小写 warning，日志完整保留。

真实源构型与 Host 的差异：现有 recorder-startup 使用原始 vision_drive.c（route ticks=132），额外显式 AUTO_TRACK=1，未强制包含 recorder_build.h，也未定义 RECORDER_TRAINING=1；DRIVE/DUAL_UART 的显式值与生产派生值均为 1，不能把仅写法不同也叫行为不同。Host 平台头和队列/IRQ/PWM 为 mock，无法证明 RTOS 抢占、DMA 或物理 PWM 时序。实施时增加实际 recorder 生成源与关键配置一致性检查，保留现有 generic 回归。

备份和新构建：当前 build_recorder.ps1 使用固定 .build/recorder-train 输出并重生成工程，先保护整个生成链才能调用；现已完成原始快照。后续 review=0 和 review=1 必须分别记录、隔离留存，不以宏值 1 冒充硬件审查完成。
