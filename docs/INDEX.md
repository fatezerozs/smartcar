# 文档目录与维护规则

**2026-11-13 NEWNEW系列分类归档：** [V1～V11逐版源码、固件与状态](../releases/NEWNEW_INDEX.md)。本次只整理NEWNEW及之后；V10/V11原发布保持，V11仍为当前NEWNEW最新候选。此前C26/WC/RINGLOG不在本次上传范围。

[NEWNEW C26 V11 提速与实车交接](changes/2026-11-12_NEWNEW_C26_V11_HANDOFF.md)。

**2026-11-12 独立NEWNEW C26 V10归档：** [完整代码、固件与交接](changes/2026-11-12_NEWNEW_C26_V10_HANDOFF.md)。保留C26；直道目标1.4m/s、实际环岛最高1.0、避砖0.55。Keil/所列Host通过，V10实车待验；独立NEWNEW不替换MPC主工程及其既往结论。

**2026-11-09进度更新：** [Phase3当前进度与交接](changes/phase3-progress-20261109/README.md)。当前处于Phase3影子上板诊断与软降级改造；V4已静态核对，near/full事件解耦已通过软件回归但未烧录。下方既往测试、板载身份和“未提交”叙述按各自时点保留；最新状态以该入口为准。

当前现状以 [项目设计与演进档案](PROJECT_EVOLUTION.md) 为准。README 只作工程入口。

选择工程与查找对应说明，请先看[主要工程索引](PROJECT_INDEX.md)；其中列出MPC主工程、采集训练、B2/B3与联合静止候选、Host规划及诊断工程。

当前维护日期为2026-11-09（北京时间）。MPC/Recorder/标定成果已合入main；主线合并身份、CI及文档口径见[主线合并与文档同步](changes/2026-11-07_MAIN_DOCUMENTATION_SYNC.md)。此前核定和更正见[MPC审核记录](changes/2026-11-06_MPC_AUDIT_AND_PUBLICATION.md)；原始数据与历史失败保持原样。

**全库状态口径：** [文档维护总览](DOCUMENTATION_STATUS.md)区分已发布成果、工作区增量、历史证据与操作参考。最新Phase2实测从[成果交接](changes/phase2-calibration-20261108/DELIVERY_STATUS.md)进入，既有11月6日CI结果不自动代表最新提交。

## 持续维护入口与阶段交付

- [Phase3影子验证待做清单](changes/phase3-progress-20261109/README.md#待做清单推进有效运动影子验证)：按顺序推进真实路径断点、影子版本核对、停机输入/记录及低速运动验证；区分首次影子必要项与主动接管后续工作。
- [项目设计与演进档案](PROJECT_EVOLUTION.md)：持续更新，含每次提交简略索引、目标、模型、现状与决策。
- [2026-11-07 / 主线合并与文档同步](changes/2026-11-07_MAIN_DOCUMENTATION_SYNC.md)：当前main身份、CI、各主文档维护范围和后续工作。
- [2026-11-06 / MPC工程核定](changes/2026-11-06_MPC_AUDIT_AND_PUBLICATION.md)：参数事实、软件更正、历史失败与发布边界。
- [2026-11-02 / C32 测试体系重构](changes/2026-11-02_C32_TESTING_FRAMEWORK.md)：C32原始工程交付、接口延期、本地/远程验收与烧录放行边界。
- [2026-10-28 / C31 软件架构硬化（pre-flash）](changes/2026-10-28_C31_SOFTWARE_HARDENING.md)：此前软件提交差异、验收摘要与继续保留的硬件风险。
- [2026-10-23 / C31 详细提交说明](changes/2026-10-23_C31.md)：此前提交快照，冻结历史结论。
- [2026-10-22 / C28 详细总结](changes/2026-10-22_C28.md)：上次提交快照，冻结历史结论。

## 当前操作与技术参考

当前主工程名为[MPC_20261106](changes/2026-11-06_MPC_PROJECT_RENAME.md)，旧`midterm_switch.uvprojx`路径见更名映射。

参考页可能包含较早变体的实验，按标题提示和适用范围阅读；不能跨工程照搬参数。

| 文档 | 用途 |
| --- | --- |
| [当前参数集与实测数据交付（2026-11-06）](changes/vehicle-parameters-20261106/README.md) | 生产 Encoder CPR/方向、控制与舵机配置、几何参考、9个原始窗口、CRC/哈希及独立与联合验收边界 |
| [采集训练版实施与使用](vision_recorder/IMPLEMENTATION.md) | 独立 RTOS 工程、持续 SD 写入、取卡/PC 导入、软件证据和待上板项 |
| [车载持续采集存储架构](vision_recorder/ARCHITECTURE.md) | 用户已批准并实施首版的设计依据；板端实时性和稳定性待验收 |
| [采集版内存与时序预算](vision_recorder/MEMORY_BUDGET.md) | 两级缓冲、SD 长尾和控制期限的候选计算，非实测 |
| [采集版未决事项与实施门禁](vision_recorder/OPEN_ISSUES.md) | 调度/所有权/协议/SD/取卡的待关闭问题和用户已定边界 |
| [2026-11-04 视觉数据架构交接](vision_dataset/ARCHITECTURE_HANDOFF_2026-11-04.md) | 已完成模块、尚缺架构、职责边界、接手顺序和验收条件 |
| [PC 视觉数据处理链条](vision_dataset/README.md) | SCRAW-1 adapter、归档/抽样/审核/Release与C31离线评测；区分主链synthetic验证、B2真实短窗回读与仍缺的审核数据集 |
| [数据链条软件验证](vision_dataset/SOFTWARE_VALIDATION.md) | 本轮真实执行、模拟审核、已测范围及正式格式/实例/真实数据未验项 |
| [测试框架与流程详细手册](TESTING_FRAMEWORK_GUIDE.md) | 完整使用/开发/维护流程、接口示例、排障与套件索引；主页 README 有直接入口 |
| [C31 连续速度规划与降级控制阶段交付](changes/c31-speed-planning-20261105/README.md) | [当前数据接入](changes/c31-speed-planning-20261105/DATA_INPUTS.md)与[Phase 3安全边界](changes/c31-speed-planning-20261105/PHASE3_BOUNDARIES.md)；近场失效清零/故障重新准入已改源码，规划器与标定门仍待完成 |
| [TESTING_ARCHITECTURE.md](TESTING_ARCHITECTURE.md) | 统一测试入口、套件分层、隔离执行、报告和数据集待接入接口 |
| [测试验收记录](testing/VALIDATION.md) | 当前main CI和MPC软件验收入口，以及保留的C32原始源码/夹具前后对照 |
| [ENCODER_SPEED.md](reference/ENCODER_SPEED.md) | 编码器3左/4右与速度环；区分赛道和历史诊断参数 |
| [C1车模暂定几何与编码器/舵机核对](reference/C1_VEHICLE_MODEL.md) | 用户原图参数、名义周长、估算轴距/轮距、已有静态端点及Host模型；最新轮端CPR见当前参数交付，实际前轮角仍缺测 |
| [DUAL_UART.md](reference/DUAL_UART.md) | 链路、队列、所有权和诊断基线 |
| [VISION_DATA_CAPTURE.md](reference/VISION_DATA_CAPTURE.md) | 停车/手推图像采集；不等于运动中图文同步 |
| [SEEKFREE_RUN_RECORDING.md](reference/SEEKFREE_RUN_RECORDING.md) | 文本导入、记录与解释边界 |
| [TRACK_TIME_ACCEPTANCE.md](reference/TRACK_TIME_ACCEPTANCE.md) | 局部/整体速度、稳定性、停车和覆盖验收 |
| [SERVO_CALIBRATION.md](reference/SERVO_CALIBRATION.md) | 舵机标定工具；当前端点以演进档案为准 |
| [BUZZER_DIAGNOSIS.md](reference/BUZZER_DIAGNOSIS.md) | B11蜂鸣器诊断参考；历史0.55V现象不代表仍故障 |
| [MOTION_DIAG.md](reference/MOTION_DIAG.md) | 独立运动诊断变体；不是C31循迹操作 |

## 历史版本资料

以下是原实验记录，不随当前版本更新数值；用于定位为什么改、为什么退回及对应证据。
新的完整说明统一写提交记录，不再给每个小修复并列增加一个“当前使用指南”。

### 版本实验

- [MIDTERM_C26_10：针对中期A→B连续弯衔接](archive/versions/C26_10_MIDTERM_MAP.md)
- [MIDTERM_C26_11：依据当前白色通道自主选择方向](archive/versions/C26_11_CURRENT_WHITE_PATH.md)
- [MIDTERM_C26_12：保留边缘白带纠偏，平滑找线舵量](archive/versions/C26_12_EDGE_RETURN_SMOOTHING.md)
- [MIDTERM_C26_13：弯中轮速配合和随行数据记录](archive/versions/C26_13_TURN_SPEED_AND_RECORDING.md)
- [MIDTERM_C26_14：复核红线路径，扩大大弯舵量](archive/versions/C26_14_MAP_AND_STEERING.md)
- [MIDTERM_C26_15：弯速小调及 B→A 验收路线](archive/versions/C26_15_B_TO_A_AND_CORNER_SPEED.md)
- [MIDTERM_C26_16：C26 控制连续整圈](archive/versions/C26_16_CONTINUOUS_LAPS.md)
- [MIDTERM_C26_17：居中、发车和单边十字](archive/versions/C26_17_CENTER_START_AND_CROSSING.md)
- [MIDTERM_C26_18 候选：出十字后的弯道恢复、长直道提速](archive/versions/C26_18_RECOVERY_AND_STRAIGHT_CANDIDATE.md)
- [MIDTERM_C26_18：圆岛进出、弯速及出十字恢复](archive/versions/C26_18_RING_AND_SPEED.md)
- [main 赛道合入与舵机/差速联动（2026-10-22）](archive/versions/C26_20_MAIN_INTEGRATION.md)
- [C26_21：提速、十字方向延续与环岛跟随](archive/versions/C26_21_ROUTE_POLICY.md)
- [C26_22：弯道驱动力与十字过渡](archive/versions/C26_22_CORNER_AND_JUNCTION.md)
- [C26_23：实车日志驱动的速度与路径修正](archive/versions/C26_23_FIELD_SPEED_AND_PATH.md)
- [C26_24：修正前馈偏大时的持续轮速偏差](archive/versions/C26_24_SPEED_TRACKING.md)
- [C26_25：应用已确认的舵机端点](archive/versions/C26_25_SERVO_CALIBRATION.md)
- [C26_26：相切圆环切入与继续提速](archive/versions/C26_26_TANGENT_ENTRY_AND_SPEED.md)
- [C26_27：修复外围补线导致整帧爬行](archive/versions/C26_27_PERIPHERAL_JUNCTION.md)
- [C26_28：移除标签限速，修复宽视野路口丢线](archive/versions/C26_28_EVIDENCE_DRIVEN_SPEED.md)
- [C29：相切入环、连续速度证据与贝叶斯入口提示](archive/versions/C26_29_ENTRY_AND_CONTINUOUS_SUPPORT.md)
- [C30：基于实拍修复相切圆环入口漏检](archive/versions/C26_30_CAPTURED_TANGENT_ENTRY.md)
- [C31：相切入口引导连续性检查与修正](archive/versions/C26_31_ENTRY_CONTINUITY.md)
- [MIDTERM_C26_6：实际编码器反向停车与起步响应](archive/versions/C26_6_ENCODER_START_FIX.md)
- [MIDTERM_C26_7：有效视觉大偏差时限幅纠偏](archive/versions/C26_7_LATERAL_RECOVERY.md)
- [MIDTERM_C26_8：参考代码路径跟踪及短时循弯找线](archive/versions/C26_8_REFERENCE_PATH.md)
- [MIDTERM_C26_9：略加大弯道舵量，优先近处居中](archive/versions/C26_9_STEER_CENTERING.md)

### 早期设计、基线与交接

- [main 架构与拨码模式](archive/history/ARCHITECTURE_MODES.md)
- [2026-10-20：无线串口与外设实车测试基线](archive/history/BASELINE_20261020.md)
- [贝叶斯赛道估计与中期开环驱动](archive/history/BAYES_REFACTOR.md)
- [FIELD_4：C12 开关、两秒启动、掉电保存](archive/history/C12_STARTUP.md)
- [2026-10-22 C31 相切入口引导连续性](archive/history/CHANGELOG.md)
- [2026年10月21日训练固件烧录与参数恢复](archive/history/FIELD_20261021.md)
- [2026-10-21 地面短跑分析与下一轮参数](archive/history/FIELD_20261021_RUN_TUNING.md)
- [四键操作及 B11 现场诊断：IO_KEYS_V2](archive/history/KEYS_AND_GPIO_DIAGNOSTICS.md)
- [C26 看线运行版](archive/history/MIDTERM_C26_SWITCH.md)
- [中期检查现场版（2026-10-19）](archive/history/MIDTERM_FIELD.md)
- [中期自主运行实施与验证](archive/history/MIDTERM_IMPLEMENTATION.md)
- [中期 A 到 B 连续循迹训练候选版](archive/history/MIDTERM_ROUTE_CANDIDATE.md)
- [中期第一阶段 训练准备与静态视觉](archive/history/MIDTERM_STAGE1.md)
- [最小运动测试：直线、往复、圆弧及动态停车](archive/history/MINIMAL_MOTION.md)
- [FIELD_2：单轮开环标定与独立电机确认](archive/history/MOTOR_BENCH.md)
- [单轮起步占空比诊断版](archive/history/MOTOR_THRESHOLD_DIAG.md)
- [中期电机开环现场版 FIELD_3](archive/history/OPEN_LOOP_FIELD_3.md)
- [完整项目说明与交接](archive/history/PROJECT_GUIDE.md)
- [外部参考与弯道/速度修订：MIDTERM_C26_5](archive/history/REFERENCE_TURN_REVIEW.md)
- [行驶开始/结束能力检查与日志接入方案（2026-10-22）](archive/history/RUN_START_END_ASSESSMENT_20261022.md)
- [无线舵机端点实测记录（2026-10-22）](archive/history/SERVO_ENDPOINT_TEST_20261022.md)
- [当前交接：2026-10-20 无线串口与外设基线](archive/history/TEAM_HANDOFF_20261020.md)
- [2026-10-22 有效成果、C26_19 与接手入口](archive/history/TEAM_HANDOFF_20261022.md)
- [当前验证记录](archive/history/VERIFICATION.md)
- [无线零接收现场检查（2026-10-22）](archive/history/WIRELESS_20261022_DIAG.md)
- [按键回退与无线控制评估](archive/history/WIRELESS_CONTROL_ASSESSMENT.md)

## 管理约定

1. “现在采用什么、完成到哪里”只维护演进档案，其他页面引用它。
2. 每次 push 新增一份 `changes/YYYY-MM-DD_<版本或主题>.md`，在演进档案添加一行简略索引。
3. 操作参考只保留仍可使用的独立主题，明确主程序和诊断/训练变体。
4. 旧实验冻结进归档；保留独有结果与证据，不不断复制旧段落到 README。
5. 移动文件同时修复相对链接；版本产物与结构化证据在原目录，原始录制继续本地保存。
6. 有代码、回归通过、实拍静态回放和实车验收分别说明，不混作一个“已完成”。
