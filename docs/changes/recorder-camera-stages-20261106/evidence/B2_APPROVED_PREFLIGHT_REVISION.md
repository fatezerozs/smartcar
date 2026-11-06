# B2 approved preflight revision

用户已批准 B2 静止相机采集与短窗口测试，确认底盘固定且执行器供电隔离；尚未换烧 B2。此记录时未访问串口、未烧录、未启动相机。

当前候选：run-20261105T194348-27184-final，取代 run-20261105T192407-50452-final 作为待烧录版本。两个目录与原有产物均保留。

改动：UART1 中断接收和 128 字节环形缓冲，避免轮询漏收突发 STOP；启动前新增 B2 STATUS，返回固件身份及 camera_started=0/session_started=0，不启动采集。Host 测试覆盖突发、溢出、FIFO、索引回绕及完整 STOP，结果见 b2-policy-irq.log。

新版 build-evidence.json 确认零错误零警告、无执行器/编码器/控制模块符号、依赖冻结一致。AXF SHA256: b6745f510531811457b873ded7c80930f76b13668651a2b51b255c99800c07df。

对应归档工程为当前候选目录 candidate.uvprojx；源码工程为 demo/recorder_b2/mdk/recorder_b2.uvprojx。归档目录应保留作为构建证据；复现构建使用 tools/build_recorder_b2.ps1 分配新目录，避免覆盖旧产物。

证据层：代码、工程 XML、JSON 为 structured_parsed；构建及 Host 测试为 computed。真实相机与 SD Session 未验收，等待用户手动换烧并运行。
