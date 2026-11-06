# C31 recorder 安全边界整改：Phase 0 / Phase 1

日期：2026-11-05。状态：基线核查与整改设计已形成，设计待确认；尚未修改固件源码。

依据为 `C:\shengchanli\for_GPT\SmartCar_C31自主停止保护与Recorder_Train安全边界整改_Codex实施任务书_v1.1_20261105.md`。用户已授权按任务书推进；任务书第 7 节要求“设计说明确认后再修改”，因此本交付提供该确认节点所需的具体设计和证据。

用户已选择 **保留零动力恢复窗口，先讨论期限依据**，因此初稿的“单帧失效立即终止锁止”建议不再作为实施政策。修订方向为视觉失效立即抑制最终双轮 PWM，在固定总窗口内等待连续新鲜近/远场证据；窗口内可以恢复原运行租约，STOP/FAULT 后始终不能恢复。候选 300 ms 总窗口与 5 个连续恢复帧仍待讨论确认，不能描述为硬件安全标定。

| 文件 | 内容 |
|---|---|
| [PHASE0_BASELINE.md](PHASE0_BASELINE.md) | 可恢复基线、历史语义、当前状态流转及核查边界 |
| [BUILD_MANIFEST.md](BUILD_MANIFEST.md) | 当前构建身份、真实工程/宏/源文件链与产物哈希 |
| [SAFETY_STATE_MACHINE.md](SAFETY_STATE_MACHINE.md) | 待确认设计、零输出/锁止、输出并发及实施范围 |
| [SOURCE_AUDIT.md](SOURCE_AUDIT.md) | 源码/既有二进制反汇编发现、中断表、待测假设 |
| [CONTROL_EVENTS_SCHEMA.md](CONTROL_EVENTS_SCHEMA.md) | 有界事件队列、会话握手、sidecar 与固件身份关联方案 |
| [TEST_REPORT.md](TEST_REPORT.md) | 本轮真实基线检查结果、MSVC 阻碍及后续测试矩阵 |

原始证据位于工程内 `.build/c31-safety-20261105-phase01/`。基线压缩包保存了 1,428 个文件的原字节，归档后逐项校验并与现场文件复核；它包含已跟踪/未跟踪源码和完整既有 recorder 构建目录。不得把它直接全量覆盖回工作区；恢复应先解包到隔离目录，再按需比对恢复，避免覆盖后续用户工作。

本轮 GCC 补充基线共 9 个 Suite 通过；MSVC 尝试为 2 PASS / 7 BLOCKED，未完成编译测试。4 个生产单元的 ARMCLANG 预处理与既有 AXF 反汇编导出成功。这些结果不代表整改已经完成，也不代表完整 Host/PreFlash 或硬件验收通过。

读取层次：原生 Markdown/C/H 直接读取，工程 XML/JSON 为 `structured_parsed`，哈希、预处理、反汇编导出和基线测试为 `computed`。没有只依据 `md_extracted` 的结论，没有 `pdf_verified` 项。未烧录、未访问串口、未连接车辆；没有 commit/push。
