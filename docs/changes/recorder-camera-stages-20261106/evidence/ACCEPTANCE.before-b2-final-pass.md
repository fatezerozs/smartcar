# 本轮交付验收与阶段门

本轮完成获授权的软件准备范围；不是整车或所有阶段最终验收。

| 阶段/要求 | 结果 | 证据与未通过项 |
| --- | --- | --- |
| P0 基线/源码/构型 | 审计交付完成 | 1060文件基线，生产源码/主线与原训练工程未改；物理项仍UNKNOWN |
| P0 内存/DMA | 静态核验完成 | 主线新MAP、既有Recorder MAP、新B2 MAP；SDRAM/DMA/栈高水位实测待做 |
| P0 哈希差异 | 可用快照区间已逐项核对 | 性能工程6项XML差异；首次变化XML前像缺失，因果无法补造 |
| B1 | SKIPPED_BY_USER | 不计通过，不生成长期吞吐/队列假证据 |
| B2 PC准备 | PASS 软件范围 | 25项Recorder/B2，33项PC回归，C生产者四条合成Session、帧重建/缺口/CRC/抽样 |
| B2静止候选 | PASS 纯构建/隔离 | 原模块复用，2:映射，B2 START门控，软停止；0错误0警告，190输入冻结；无执行器/Control符号 |
| B2真实Camera连续Session | NOT_RUN | 未烧录、未串口命令、未相机/SD并行；真实规格、帧率、争用、完整Session仍UNKNOWN |
| C0规程/模板/误差 | 准备完成 | 84行unknown模板、规程、不确定度、待审核参数表；未实际量测/动作，不写生产配置 |
| B3/C1/D/E | NOT_AUTHORIZED / NOT_STARTED | 缺真实Camera/运动Session、本车参数、A/B及审批，不准production-ready |

历史完整Host的2项reviewed失败保留。本轮专项通过不替代完整Host/PreFlash，不修改旧断言来掩盖它们。没有真实控制deadline/PRIMASK/WCET结果。

## 下一批次的具体审核对象

可审核的候选是 `demo/recorder_b2/mdk/recorder_b2.uvprojx`，不是原recorder_train或C26。最终构建位置见evidence/b2-candidate-location.txt，AXF/BIN/MAP/日志/输入哈希齐备。B1豁免不自动授予硬件权限。
若用户单独批准：先隔离执行器电源、固定底盘、确认接线及SD空间/已有文件，烧录/运行该候选；ready后另行确认允许发送一次B2 START，记录约3s/100观察事件的软停止窗口及B2 STOP，保留新Session，再全量导出做PC重建。任何丢帧标incomplete，不延长测试、不进入车辆运动、不自动用该结果放行B3。
继续做C0实际量测/Encoder读取/舵机动作、控制并行静止验证及后续阶段时，各自需要原任务书规定的人工批准。

## 读取与证据可信层

任务书/代码是原生文本，XML、JSON、CSV原生解析属structured_parsed；编译、MAP/内存、CRC、缺口、哈希和Host执行属computed。无md_extracted；无PDF，pdf_verified不适用。合成Session均明确synthetic=true，不混入真实相机证据。真实未知项逐项保留UNKNOWN。

## 自审

本轮只改3个既有文件：Recorder存储模块的可配置profile hooks、PC archive完整性边界、PC CLI入口。其他改动均为新增隔离入口/工具/测试/文档/产物。原有未提交内容保留，task-incremental.patch只含本轮增量；没有改主控制、生产参数、原Keil工程宏、旧断言、ACL、Git历史或全局配置/技能。硬件操作为零。

## 2026-11-06 实板状态更新（覆盖上文 NOT_RUN/硬件操作为零的历史状态）

用户批准且确认手动换烧后，已通过 COM14 执行一次 B2 静止短窗口。板端关闭成功，100 帧写入/同步，零报告错误，Session 为 2:/R0000001；串口已关闭。完整 Session 导出和 PC 校验待完成，B2 不标最终 PASS，后续运动阶段未放行。详见 evidence/b2-hardware-20261105T194852617Z/REPORT.md。

## 2026-11-06 PC真实Session验证更新

R0000001已完整只读下载，100帧CRC/PNG/序号/时间戳/END/manifest通过，真实数据链路PASS。完整B2尚待仪表化短窗补CPU/Writer/IRQ/CSI读回，候选已构建，见B2_CAMERA_SESSION_VALIDATION.md。基线复核发现两个旧SD工程uvoptx另有变化，原因不作无证据归因且未回退；原生产/训练工程及主线源码保持基线。

## 2026-11-06 仪表化首次实板失败

R0000002的测量输出损坏并失去后续响应，本轮拒绝验收，不将其CPU/IO数字用于最终结论。已移除目标库递归qsort，新增哨兵/固定空间排序/报告END/栈余量检查，v2构建通过，待用户换烧后重测。原R0000001数据链路PASS保持，完整B2仍待定。根因、日志与修正版位置见 evidence/b2-hardware-20261105T201919543Z/REPORT.md。
