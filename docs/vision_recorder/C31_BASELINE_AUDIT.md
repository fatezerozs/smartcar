> **维护口径（2026-11-09）：** Phase2实测与诊断工具已单独发布于bd94f27；没有据此验收Recorder运动写卡、Camera/SD/PI组合或正式训练数据集。现有静止/软件结果按本文原构型阅读；其他未提交Recorder/Phase3实现不能写成main已发布或当前车载配置。 统一状态与历史适用范围见[文档维护总览](../DOCUMENTATION_STATUS.md)。

# C31 采集基线审计（2026-11-04）

本轮用户明确批准 PC 软件链条及 synthetic/mock 验证。此报告只审查固件现状，
不实施 Recorder、不烧录、不访问串口/调试器/真实 SD 卡。固件正式格式仍未冻结。

## 基线与既有工作

当前 `main` HEAD：`de14157`。本地 `origin/main` 缓存多一条 `018a1e1 Create 1103`，
其范围是新增根目录 `1103` 一行；本轮没有同步或重写该提交。未联网确认最新远端。
起始工作区为 switch 工程 M 标记（`git diff` 无内容差异），另有前轮本地历史资料。
均保留；本轮新增 PC 工具和文档，没有改变生产控制或原测试框架。

| 提交 | 日期（北京时间） | 范围 |
| --- | --- | --- |
| `79c8ec6` | 2026-10-28 18:24:55 | C31 启动/帧/执行器/PI 软件硬化及验收归档 |
| `d78eb9d` | 2026-11-02 15:28:31 | C32 测试体系、夹具拆分、性能接口；生产视觉/控制未修改 |
| `1a96c8f` | 2026-11-02 15:54:45 | 补录 main CI 验收文档 |
| `de14157` | 2026-11-02 16:28:46 | 测试框架与流程手册 |

提交范围来自本轮 `git log` / `git show --stat`，不是沿用旧对话推断。

## 可定位事实

| 项目 | 当前源码证据 | 结论 |
| --- | --- | --- |
| 图像/配置 FPS | `libraries/zf_device/zf_device_mt9v03x.h:77,78,85` | 188×120，GRAY8 配置，默认 FPS=50；不是实际测量帧率 |
| 双缓冲 | `libraries/zf_device/zf_device_mt9v03x.c:69,70` | 两份 22560B、DTCM section、64B 对齐 |
| 完成回调 | 同文件 `mt9v03x_finished_callback:393`，回收另一缓冲 `:401,406` | 无前台持有/释放协议；不能直接交给不定时长 SD 写入 |
| 序号/时钟 | 同文件 `:414–423`；`demo/code/board_camera.c:capture_clock_ms` | 回调时读取 GPT 毫秒并递增序号；不是曝光时间，也不保证所有 Sensor 帧均被计数 |
| SDK 双完成 | `libraries/sdk/drives/fsl_csi.c:865–870` | 双 DMA done 同时出现可跳过；回调计数不能证明源头零丢帧 |
| 视觉快照 | `demo/code/vision_frame.c:11–25` | 复制前后核对指针/序号/完成标志；开中断复制不能独立证明 DMA 不覆盖 |
| 主循环 | `demo/code/app_runtime.c:261–296` | 从完成标志复制并调用 `vision_midterm_process`，处理帧不是全部 Capture 帧的排队流 |
| 控制/保护 | `demo/code/vision_drive.c:138,139,205`；`vision_profile.h:145` | PIT 安全采样、前台 PI、帧超时 100ms；同步存储可能影响前台期限 |
| IRQ | `demo/user/src/isr.c:49` CSI、`:72` PIT1 分支 | 没有应用层存储线程；不能由“non_blocking”文件名推断写文件不阻塞 |
| SD 管脚/初始化 | `libraries/zf_driver/zf_driver_sdio.c:50–90` | USDHC1 CMD/CLK/D0..D3、B23/D4 配置及 `sdio_init` 存在，含卡插入等待 |
| 工程引用 | `demo/mdk/competition/midterm_auto-track.uvprojx:343,2130` | FatFs、SDMMC include/source 在工程中；不等于已调用应用记录链 |
| 应用调用 | 对 `demo/code`、`demo/user` 搜索 `sdio_init/f_mount/f_open/f_write/f_sync/f_close` | 没有匹配；当前 main 没有接通 SD 记录 |
| 文件写入后端 | `libraries/components/fatfs/source/fsl_sd_disk/fsl_sd_disk.c:45` | `SD_WriteBlocks` 同步包装；SDK 存在卡忙等待和超时 |
| 同步语义 | 同文件 `:109` 的 `CTRL_SYNC` | 直接返回 RES_OK；不能把它当作已证明介质持久化完成 |

`main.c` 的 `vision_storage_board_init` 属于车辆配置保存，不是 SD 图像记录接口。
当前主应用没有引入 RTOS 任务；SDK 存在 OS abstraction 不代表应用有独立低优先级写线程。

## 容量计算与未知

按源码配置计算（非实测）：188×120=22560B/帧；50FPS=1,128,000B/s；
32 帧=721,920B=0.64s。仅 CSI 双缓冲+前台灰度+二值图约 90,240B，未包括
视觉状态、堆栈、其余缓存。依据 `demo/mdk/scf/MIMXRT1064xxxxx_flexspi_nor.scf`
的区域定义不能推出实际空闲 RAM；本轮未建立新固件 map、DMA 可达性/Cache/WCET 证明。

Recorder pool 至少需 `ceil(actual_capture_fps × measured_write_tail_seconds) + margin`
帧，且必须计入快照/总线/控制预算。实际 Capture FPS、SD 长尾、可用 RAM、
CSI/USDHC 并行总线影响均未知。现阶段无法证明在原主循环里直接 `f_write` 安全。

固件风险 R1（DMA 所有权）、R2（时戳可信度）、R3（IRQ/PWM 时序）保持开放。
PC 链条采用独立暂定格式验证，不把当前双缓冲接口说成已经能完整记录每一帧。

## 阅读与证据

主计划为原生 Markdown 全文直读；两份 v1.1 上游任务书按目录和有关章节定点读取，
未声称所有条款已逐项验收。源码直接读取，工程 XML/JSON 为 `structured_parsed`，
Git 状态与上述算式为 `computed`。无仅基于 `md_extracted` 的关键数字，未使用
`pdf_verified`，无硬件实测。当前芯片/SD 板级和介质行为仍是未验证边界。
