# P0 现状基线与阶段边界

日期：2026-11-06，HEAD `de1415743b004160f520d385c9e3e63008f52007`，包含既有未提交改动，不是 clean main。
任务书是原生 Markdown 原文读取，没有 OCR/MarkItDown 抽取。用户当前“按照任务书完成，B1可跳过”授权软件实施，覆盖文档“审核稿未授权实施”的草案状态；硬件和后续阶段门继续适用。
本轮范围：P0 审计、B2 静止候选/协议/PC 准备、C0 规程。B1 是 SKIPPED_BY_USER，不是 PASS；B3/C1/D/E 未实施。

开始记录 `evidence/git-status-before.txt`、HEAD、tracked-before.patch、1060 个原文件哈希；只按 Git 跟踪清单及明确的 Recorder/SD/PC/测试/相关文档路径定位。没有扫描 .build/video-deps、受限临时目录或整个项目，已有 .build 仅读取具体命名的 MAP/provenance，本轮输出在新限定目录。没有 Git reset/clean/checkout/revert/add/commit/push，未改 ACL。

## 当前构型与调用链

| 对象 | 实际入口/关键关系 | 结论 |
| --- | --- | --- |
| MIDTERM_C26_31 | demo/mdk/competition/midterm_switch.uvprojx；VISION_MIDTERM_AUTONOMOUS=0，VISION_ROUTE_SWITCH=1，VISION_MIDTERM_ROUTE_TRIAL=1，VISION_ENCODER_ENABLE=1 | 主线 owner 不变，C26 OFF→ON 授权、OFF 撤销；不得拿 AUTO_TRACK/Recorder 替代 |
| Recorder TRAIN | [demo/recorder/recorder_main.c:31](../../../demo/recorder/recorder_main.c#L31) → PWM/方向/Servo 初始化、Encoder、Camera、Control/Capture/Vision/Storage/Link 五任务 | 不是静止 Camera 固件；AUTO=1/RTOS 控制流程不同 |
| Capture | [demo/recorder/recorder_capture.c:39](../../../demo/recorder/recorder_capture.c#L39) → rec_capture_prepare → 替换 DMA 地址+DSB → publish → Capture task复制/零等待入队 | 源码显示 producer 无 FatFs 等待；实际复制时间/仲裁 UNKNOWN |
| Writer | [demo/recorder/recorder_storage.c:180](../../../demo/recorder/recorder_storage.c#L180) → f_mount/create_session/open_new → FRM/GAP/END → sync/close/manifest | 单独任务，错误隔离；卡操作超时后 quarantine，不重新借出 DMA 缓冲 |
| SD | recorder_disk → SD_Init/Read/WriteBlocks → 既有私有 host/usdhc adapter → USDHC1 | Recorder 是 1-bit/3.3V/最高25MHz；不能拿性能版49.5MHz直接代替其吞吐 |
| PC | tools/vision_data/scraw_v1 → archive → 全量 A0/A2/frames.jsonl → continuous → sampling/Label Studio | 沿用 SCRAW-1；原始字节不删除，抽样单独保存 |

直接复用：recorder_core 的 ownership/CRC/序列化、Capture 队列、Writer/SD OSA、已校验私有 SDK adapters、PC archive/sampling/tasks。
需适配：静止入口/任务、SD 卷、构型身份、停止原因、完整性边界与连续审计。不可当作等价证明：训练构型的控制启动、AUTO_TRACK 一 boot 一租约、既有 SD 单独性能和控制并行实时性。

## 本轮具体发现与处理

1. 原训练 Writer 使用 0:，diskio.h 的 SD_DISK 为 2（SDDISK=2）。Host mock 未校验真实物理盘号，不能证明板上能挂载。新增 RECORDER_VOLUME 参数，原默认0:保留以免改变既有训练构型；新 B2 显式选择2:。旧训练工程的该缺口仍存在，不能直接放行。
2. 旧 PC archive 检查计数但未核对 END 的首末序号与实际帧范围，存在 CRC 合法而边界不一致仍 complete 的缺口。本轮新增核对和回归，异常降为 incomplete。
3. Camera 参数是配置事实：[libraries/zf_device/zf_device_mt9v03x.h:77](../../../libraries/zf_device/zf_device_mt9v03x.h#L77) 为188×120，FPS_DEF=50；[libraries/zf_driver/zf_driver_csi.c:193](../../../libraries/zf_driver/zf_driver_csi.c#L193) 为1字节/像素、8-bit总线。真实输出宽高/帧率/曝光读回均 UNKNOWN，未读串口或启动相机。
4. 断电结论更新：先前串口导出五文件、PC 独立4640块校验通过已有 evidence/readback-20261105T183635Z-9052/PERSISTENCE_REPORT.md；证明的是同步关闭后完整断电的数据保留，不是写入中掉电，也不是 PC读卡器独立读盘。
5. SDK Camera SCCB 使用已有 zf_device_config.lib，源代码不可见；原库哈希纳入候选构建冻结，不能宣称已审计其内部实现。

## 构建、历史失败和证据级别

主线纯构建位于 .build/recorder-camera-stages-20261106/primary-01：ARMCLANG6.21，0错误0警告；原 C26 项目 SHA256 未改。MAP 没有 Recorder task、RTOS scheduler 或 sp_plan 的已链接代码。本轮没有修改生产控制、Keil宏或生产参数。
静止候选使用独立入口和输出，详见 B2_CAMERA_SESSION_VALIDATION.md；它不是 C26 集成。
已有完整 Host 基线的 auto-track reviewed 和 recorder-startup reviewed 两项失败按原报告保留；未改旧断言，本轮没有重跑完整 Host/PreFlash，不能称全部Host已绿。
证据：native源码/Markdown、structured_parsed XML/JSON、computed 编译/MAP/Host。没有 pdf_verified 或 md_extracted。相机、SDRAM实际可达性、CSI/SD并行、真实 PRIMASK/WCET、车辆控制 deadline 均 UNKNOWN，故 P0“审计交付完成”不等于所有物理项通过。
