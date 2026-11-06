# Phase 3 首批安全边界改动：不依赖物理标定的子集

日期：2026-11-06，Asia/Shanghai。用户询问是否可以在Phase 2未完成时推进独立内容，本轮据此完成以下软件子集。**Phase 2与Phase 3并行推进；不是Phase 2已通过，也不是Phase 3全部完成。**

## 1. 范围、构型和实际变化

主验收构型仍是`MIDTERM_C26_31`，实际工程现为[competition/MPC_20261106.uvprojx](../../../demo/mdk/competition/MPC_20261106.uvprojx)，原名`midterm_switch.uvprojx`，见[后续更名记录](../2026-11-06_MPC_PROJECT_RENAME.md)。当前工作区为`feat/vision-recorder-training`，HEAD为`de1415743b004160f520d385c9e3e63008f52007`，存在此前未提交工作；本次只在其基础上提交可复核增量，没有切换分支、commit或push。

| 生产模块 | 改动 | 不需要Phase 2参数的原因 |
| --- | --- | --- |
| [vision_control.c](../../../demo/code/vision_control.c) | near观测无效时，partial分支仍可保留原有有界转向引导，但电机需求固定清零；route构型拒绝旧/半域歧义帧序号 | 有效性、身份、零输出是逻辑边界，不是速度/制动阈值 |
| [vision_actuator.c](../../../demo/code/vision_actuator.c) | route驱动在发布轮速请求前清双电机需求与PI；最终writer在PI/cache之后再次检查`observation_valid`并强制双零 | 不依赖CPR、滚动周长、地面映射或实际转角 |
| [vision_startup.c](../../../demo/code/vision_startup.c) | 仅稳定C26 OFF允许清除视觉/帧故障；混合、严重及未知故障不自动清除；ON故障锁住重新准入；保留帧发布水位 | 故障分类、物理授权和新鲜帧计数不需要新增物理阈值 |

没有将`.4m/s`、`.8m/s²`、`600ms/.24m`等Host参考值写入生产。未新增300ms/5帧候选阈值；本轮保留当前已有的启动帧数、恢复计时、开关去抖、相机/Encoder超时及速度配置。

## 2. 近场失效的三层封口

原partial分支在`observation_valid=0`时仍可能给出cruise的45%/60%需求。现在该分支只保留既有转向计算，`duty=0`，不因当前partial或历史转向恢复驱动。

输出层另外做两次独立处理：

1. 完成现有转向/轮速混合后，只要route的当前近场观测无效，就把两轮需求置零，并调用`vd_speed_stop()`清除两轮请求、目标、两个PI状态及输出缓存。零请求不等待下一Encoder样本。
2. `output_commit()`在现有来源授权/故障检查和PI/cache之后，再次以同一近场有效标记强制两个最终电机值为零。被上游错误保留的非零duty不能绕过此处。

这一分支保留原运行租约，不自行重新arm；短暂近场失效后若在原有恢复边界内重新有效，可沿原路径恢复。STOP/故障/模式撤权仍走`vd_output_zero()`，撤销授权并递增epoch，不能借视觉恢复重建授权。舵机继续使用已存在的有界命令映射，未增加实际转角模型。

**有效性范围仍有边界：**本轮使用现有`observation_valid`判定，现有near采样可能包括模型化单边道路；没有完成新的near/far质量标签、严格可信距离或物理曲率判定。此次保证的是“现有判定已无效时没有partial驱动旁路”，不是“所有near-valid判定都已经真实数据认证”。

行为变化会影响依赖partial低速通过的路口：原本继续爬行的无效近场片段现在零驱动。是否需要改善视觉有效性、是否影响赛道通过率，必须用真实图像/运动回放继续评估；不能用本次Host通过宣称整车性能提高。

## 3. 故障清除、C26与帧水位

| 情况 | 本次源码行为 |
| --- | --- |
| 上电OFF / ON但尚无有效道路 | 保留现有等待/启动规则，不迁移AUTO_TRACK启动器 |
| ON运行后短暂近场无效，尚未形成锁存故障 | 原恢复租约内双电机零输出；当前有效道路恢复后可沿原授权继续 |
| 视觉/帧故障锁存，C26始终ON | 新图像不能清除故障或恢复运行 |
| 故障后稳定OFF，再ON | 仅当故障集合属于VISION/FRAME时，OFF可清理该集合并重置准入历史；后续仍需现有新鲜有效帧数、Encoder、配置、硬件/模式等所有前置条件 |
| Encoder、config、numeric、offset、runtime、command、external或未知位；含视觉混合故障 | 普通OFF和OFF→ON均不自动清除；速度层Encoder故障也不被自动重新enable清掉 |
| UART `c`显式清故障 | 保留既有人工清除能力；ON时观察到故障已锁住route重新准入，因此`c`不能代替物理OFF→ON，也不能恢复旧输出 |
| UART `a` / heartbeat / C27变化 | 不获得route启动权；`a`继续拒绝，C27不成为C26启动器 |
| OFF短抖动、模式未稳定 | 不构成故障确认或重新准入 |

OFF确认清理控制器时，只清准入/恢复历史，保留此前的`have_frame/last_sequence/last_frame_ms`发布水位。再配合route的uint32模序拒收，旧帧、重复帧和半域歧义序号不能计作新的准入证据；合法回绕仍按既有半域顺序处理。没有把“清故障”解释为“允许把旧帧再消费一次”。

本次保留已有显式故障处置流程，不声称普通OFF能够修复硬件错误。外部严重故障、STOP与输出撤权的既有epoch机制没有扩大授权范围。

## 4. 测试修改与结果解释

修改的测试没有删除场景：

- [test_c26_baseline.c](../../../tests/planning/test_c26_baseline.c)：保留历史suite ID，改为当前边界回归；增加8类严重/未知位及各自混合视觉故障、真实速度层反向故障、partial清零、非零duty绕行、撤权后的旧RUNNING状态、旧帧重放、不足帧、OFF抖动、UART clear不准入等覆盖。
- [test_route_switch.c](../../../tests/vision/test_route_switch.c)：原图像恢复后自动启动预期改为必须OFF→ON；保留启动、二次授权、STOP、C27、长时间运行及反向反馈保护场景。
- [test_turn_route.c](../../../tests/vision/test_turn_route.c)：原partial非零驱动预期改为双零目标/缓存/PWM；保留灰度输入、转向方向/平滑、原失效片段、正常道路及弯道回归。故障后的重新授权开始新物理run，ON期间旧trace保持冻结。
- [test_switch_runtime.c](../../../tests/vision/test_switch_runtime.c)：通过真实scheduler/transport/vision/PI路径，9个模拟场景覆盖启动、STOP、相机停流、相机恢复仍锁止、稳定OFF→ON恢复、短OFF抖动拒绝、C27忽略与停止后trace导出。

旧`c26-baseline-characterization`的suite ID保持兼容，catalog名称/scope和套件说明已更新，不再把两个旧GAP当作当前预期。旧阶段日志、原始实测数据和历史证据不改写。所有CHECK次数包含重复控制循环，不作为独立场景数。

最终专项运行`20261106T095413-5d3bf1a18d`：environment/compiler及上述4个套件，共6节点PASS。其中route-switch为301063次检查，turn-route为371624次检查；硬件为模拟IO，不是实车通过。

最终完整Host运行`20261106T095539-b248b49cf9`：50节点中46 PASS、3 FAIL、1 SKIPPED，95.501秒；SKIPPED为缺数据的performance-host。逐项结果见[本阶段证据](phase3-boundaries-evidence.json)。完整计划包含既存失败，**不能标记整体Host、PreFlash或发布放行通过**。源码与副作用审查发现的改动只对本阶段范围负责，不将其他工作区实现算作本任务交付。

## 5. 既存失败的改动前对照

| 套件 | 观察结果 | 对照证据 |
| --- | --- | --- |
| auto-track / reviewed | `test_auto_track.c:113`，near-only场景的RUNNING/有效性断言失败 | 用本轮前3个生产模块的精确备份替换编译输入，保持其他输入/宏相同，复现相同stderr与非零退出 |
| recorder-startup / reviewed | `startup_harness.c:113`，`saw_running && saw_pwm && vision_drive_is_local()`失败 | 同样用改动前3模块重编译，复现同一断言 |
| recorder-tools | `prepare_recorder.py`拒绝源码锁与`demo/code/vision_drive.c`不一致 | 该文件本轮未修改，当前哈希与pre-turn哈希一致；不更新锁来掩盖失败，也未声称生成的旧训练候选已吸收本轮改动 |

上述对照是相对于本轮开始时的dirty工作区，不是干净HEAD全量基线；完整命令、退出码和stderr摘要保存在JSON，私有可执行文件/日志在`.build/phase3-boundaries-20261106`。这些失败仍是未关闭项，不能因不由本轮引入而忽略。

## 6. 主构型纯构建与产物

使用[audit_build.py](../../../tools/speed_planning/audit_build.py)隔离复制实际工程与依赖，拒绝启用构建钩子，只修改产物/Listing输出目录。最终目录为`.build/phase3-boundaries-20261106/final-switch`，Keil目标`nor_sdram_zf_dtcm`，0错误0警告：

| Code | RO-data | RW-data | ZI-data |
| ---: | ---: | ---: | ---: |
| 89624 B | 13792 B | 212 B | 196648 B |

原工程宏保持autonomous=0、route-switch=1、route-trial=1、encoder=1、training=1。AXF/BIN/MAP/反汇编的实际字节数与SHA256见证据JSON。未改原Keil工程，未打开调试会话、下载、复位或访问串口；未替换任何已交付的联合静止测试候选。

本轮是带3项安全边界修改的纯构建产物，**不是已经启用新规划器的实验固件，也没有获得烧录/实车放行结论**。不将主机PWM记录或寄存器提交逻辑当作真实PWM重载时序、停车距离或制动能力。

## 7. 尚未完成的Phase 3内容

1. 空间规划、jerk轨迹、far-only限速/时距预算及CONTROLLED_STOP尚未接入主构型；现在的far-only仍是原控制行为，不能称为完成v1.4有限降级策略。
2. route near-valid/far-valid与质量/可信距离定义仍须数据审查，生产米制模型/制动/实际轮角不得由Host参考自动补全。
3. 主构型输出准备和PI计算外围仍有原有PRIMASK区间。本次没有简单移除中断屏蔽，也没有完成不可变快照、完整候选版本复核与短提交区重构；不能宣称已达到WCET/中断延迟目标。
4. Recorder并行写入、真实同输入A/B、误停/制动/圈时及板端C26重新准入验收仍未完成。当前没有远程CI、MSVC或ASan新增结果。

因此当前可以确认“Phase 3已有独立安全子集进入源码”，不能确认“Phase 3完成”或“可直接烧录跑车”。后续继续以[Phase 2数据入口](DATA_INPUTS.md)和本阶段未完成项作为推进边界。

## 8. 读取层、保存与增量

C/Markdown直接原生读取；Keil XML、JSON和测试报告为`structured_parsed`，Host运行、改动前对照、产物哈希和文件保存检查为`computed`。本轮没有依赖`md_extracted`给出精确公式/数值，无新增`pdf_verified`或硬件测量。全局配置、技能和持久Agent行为未改。

本次增量审查包括生产3模块、4个专项测试、catalog/套件说明及主文档入口。原始交付目录、参数profile、库与原Keil工程按pre-turn哈希保护；变更清单、源码哈希见证据，[本轮精确增量patch](phase3-boundaries.patch)相对于本轮开始时的工作区，另外两份机器证据/patch文件不递归包含自身。工作区原有未提交实现保留，不混入本轮提交声明。
