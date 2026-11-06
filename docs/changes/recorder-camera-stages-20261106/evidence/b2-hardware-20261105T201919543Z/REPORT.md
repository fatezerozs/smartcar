# B2 仪表化首次实板：拒绝验收与修复

时间：2026-11-06 04:19:19—04:20:02（北京时间，详见UTC事件日志）。

## 实际动作

用户手动烧录后批准开始，COM14核对 metrics=1 身份，发送一次B2 START。新Session为2:/R0000002。记录达到100观察事件，早期drop/pool/ambiguous/error均0，队列高水位34。随后进入仅在storage_state==4执行的测量报告路径；该路径在源码中位于Writer完成sync/close/unmount之后。

报告中write有306调用，而sync/close出现sample_overflow=4294967295、错误字段为RAM地址型数值、总耗时异常大等不可能组合。没有收到最终state4状态行或后续命令回复。20秒处主机发送一次B2 STOP，35秒超时后关闭COM14，没有第二次START、复位或烧录。

原始result.json保留board_closed=false，因为旧程序在测量报告之后才打印关闭状态。源码路径表明已经进入关闭后的报告，但R0000002是否完整必须独立PC读取，不能只凭该路径判PASS。当前Session尚未导出，所有测量数字均不进入最终性能结论。

## 根因分析与证据强度

已确认的有害路径：ARMCLANG目标库qsort存在递归自调用，每层PUSH {r1-r9,lr}为40字节。原始反汇编保留于上级 evidence/b2-metrics-disassembly.txt，qsort位于0x80000462。大量相等值会产生线性递归深度；本次write的P50=0，306个非负整数样本中至少153个为0，这类输入对应的递归栈风险超过4KiB。

原MAP中io位于0x2000e0e0、6240字节，Monitor栈位于0x2000ff40、4096字节，栈向低地址增长。异常损坏首先出现在邻近Monitor栈的后两个统计对象，符合排序递归越界覆盖的模式。旧Host使用PC标准库qsort，没有覆盖Keil目标库的栈行为；单纯Host通过不能排除此问题。

因此已移除这条足以产生越界的路径。没有读取故障寄存器或板端内存快照，不能声称已经捕获精确的最终HardFault/栈溢出Hook停机指令；根因解释有源码、目标反汇编、MAP和异常模式支持，修复效果仍需实板重测。

另发现原CSI解析器错误要求寄存器为188×120。zf_driver_csi.c的csi_init为宽度非8倍数寻找总字节数不变的分组，188×120=22560被配置成240×94，bytesPerPixel=1，8-bit数据总线。这与本次读回一致，不应将其判为图像规格错误。解析器已依据源码修正，错误输入仍拒绝，没有修改旧Recorder断言来掩盖失败。

## v2 修复及验证

- 移除qsort，固定空间排序最多512元素，最多130816次移动，无递归；MAP禁止链接qsort。
- 统计对象加入前后哨兵及n<=512检查；报告前后均检查，非法状态只输出ERROR。
- 正常关闭状态先于测量输出；结束必须有B2METRIC END及报告后至少128 words栈余量，否则主机拒绝验收。
- 固件身份升级metrics=2，测试脚本不再对metrics=1发送START。
- Host覆盖512相同值、512逆序、零样本、溢出、计数越界、统计哨兵破坏；3项解析测试通过，真实损坏日志明确拒绝。
- 修正版Keil0错误0警告，冻结依赖闭合，无执行器/Encoder/Control业务符号，MAP无qsort。

候选目录：C:/shengchanli/smartCar/smartcar/.build/recorder-camera-stages-20261106/run-20261105T202447-47412-final/。
对应candidate.uvprojx、Objects/recorder_b2.axf、BIN、MAP、日志和输入清单均保留。旧故障构建已新增RETIRED_METRICS_STACK_FAILURE.md标记，旧产物不覆盖。

## 当前结论与后续

本轮仪表化验收FAIL，完整B2不通过；原R0000001全量PC校验通过事实保持。请保留R0000001和R0000002。

需用户手动换烧v2并Run后，沿用静止隔离条件和已批准窗口执行一次补测；新Session达到关闭且测量报告完整后，直接只读下载R0000002与新Session，无需再次复位。PC校验与测量均通过后，再逐项裁决B2；不进入B3/运动或生产控制。

证据层：源码/MAP/日志/JSON为structured_parsed，Host/反汇编/字节布局和采样计算为computed。没有故障现场内存或精确终止PC，修复尚无实板验证。没有修改原生产工程、ACL、Git历史或全局代理配置。

## 后续只读回读更新

在用户换烧metrics=2并完成R0000003后，已无复位地只读导出R0000002至C:/shengchanli/smartCar/b2_real_session_20261105T203027653Z/。100帧完整，CRC/序号/时间戳/PNG重建通过；这验证了旧故障发生后保留的文件内容，不把旧损坏测量翻为PASS。负对照联合审核仍拒绝旧测量，见上级evidence/b2-negative-retired-metrics-acceptance.json。
