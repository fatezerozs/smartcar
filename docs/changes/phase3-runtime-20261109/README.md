# Phase 3：C26调度、原运行PI与唯一最终writer的软件接线

**2026-11-09进度更新：** [后续上板与事件解耦进展](../phase3-progress-20261109/README.md)。当前处于Phase3影子上板诊断与软降级改造；V4已静态核对，near/full事件解耦已通过软件回归但未烧录。下方既往测试、板载身份和“未提交”叙述按各自时点保留；最新状态以该入口为准。

2026-11-09，北京时间。完成了前轮剩余的主要软件接线：原生快照→capture历史→共享监督/规划→原`vision_speed.c`中的双PI实例→原唯一最终writer。**这是可选编译、显式配置的运行链路，已通过Host模拟和目标构建；主工程默认关闭，尚未上板运行。** 工作仍在`docs/main-documentation-20261107`未提交工作区，不是main已合入或板载固件更新。

用户要求减少过度保护、优先性能和稳定运行。本轮没有调整生产增益、速度、采样/故障超时或恢复门限；计算尽量留在前台和IRQ屏蔽区外，不增加UART/SD写入、任务、动态分配或第二个PWM writer。

## 连接与所有权

| 位置 | 本轮行为 |
| --- | --- |
| `vision_planner_runtime` | `vision_drive_poll/observe`中的同一前台所有者处理视觉与已有编码器快照；采样序号不变且无新帧时跳过规划，不重复积分。有效新帧缺capture路程括号时保留上一接受帧，明确near失效立即转交监督层。 |
| 配置与目标源 | `vision_drive_planner_configure`只接受停止、无输出授权/故障/探针/诊断运动的状态。OFF/SHADOW选择旧源，CONTROL选择米制源；运行中不可切换，无自动回退。地图、路程、规划必须同一revision；配置显式提供，无固化标定或串口启用命令。 |
| 原运行PI | 复用原`pi[2]`、`ws_pi_step_ff_ms`及已有采样/严重故障处理。米制目标直接采用ticks/10ms，绕开旧permille换算、经验转向分配和旧目标斜坡；显式分轮前馈/PWM cap，前一轮的诊断PI模块不参与实际输出。舵机仍由原视觉控制路径提供有界指令。 |
| 最终输出 | 只在原`vision_actuator.c`写PWM。监督/候选复核在IRQ屏蔽区外完成，生成版本/序号/剩余时限token；区内只检查整数身份、时限、物理授权并读取原PI缓存。独立实际PWM cap、near无效清双PI、STOP/模式切换优先全部保留。 |
| 撤销与重新准入 | 新物理输出epoch建立新监督会话；STOP锁存不能在同一ON授权内自行重启，需原C26 OFF→ON。严重encoder故障保留；原探针/诊断运动不能与米制目标源同时占用输出。 |
| 查询 | `vision_drive_planner_info`提供内存状态和调用/规划/等待计数。本轮没有新增板端遥测或真实时序测量。 |

`vision_planner_build.h`仅提供轻量开关，避免旧诊断构建经`vision_speed.h`引入共享内核的相对路径依赖。默认`VISION_PLANNER_RUNTIME=0`、`VISION_PLANNER_ACTIVE=0`、`VISION_PLANNER_PROFILE_REVIEWED=0`。影子构建只启用runtime；控制构建还需active及profile开关，随后仍需RAM配置和原物理准入。Host/隔离控制构建中的review开关是显式测试声明，不是本车标定或实车批准。

## 验证与兼容性

- 完整GCC optimized Host：`20261109T010440-a3ed2835fc`，**57 PASS / 1 SKIPPED**。`performance-host`仍缺数据集、Ground Truth和批准基线，未产生性能结论。没有新增MSVC/ASan或完整PreFlash通过结论。
- 新`planner-runtime`包含CONTROL和SHADOW两个真实宏构型，分别6442/413条断言通过。覆盖原运行PI的直接5/10计数目标、100次重复poll不增加规划/改变PI输出、独立200‰上限、停止中切源、near清零与恢复、far受控停止及实际零反馈确认、OFF→ON、严重故障保留；还在输出/PI四个阶段插入STOP和模式切换，并验证提交前新采样、时限到期均拒绝旧结果。
- 上述几何、尺度、前馈、速度和监督预算全部是显式合成测试输入，不建议用于实车。测试没有证明轮速跟踪、物理制动、误停率或MCU耗时。
- 选择运行`static-safety/project-structure`：`20261109T010440-89dbb0ce8e`，连同environment共**3 PASS**；这不是整个PreFlash profile。
- 修复前轮静态失败项：将`test_speed_response.c`和`test_heartbeat_response.c`的共有IO setup移至`response_io.h`，删除测试C互相包含/重命名main。检查工具现在分别执行完整响应和心跳场景。既有冻结候选与本轮最终源码重新生成的私有候选均通过：响应14414、心跳815条断言，451行native trace CRC通过。使用新输出目录，保留原候选Host证据。
- `Recorder`源码锁因新增条件编译hook按预期拒绝了driver变更。审核后，在相同当前头文件下，对比本轮前/后的driver和actuator，C26及Recorder默认关闭构型共4项预处理结果相同；仅更新`demo/recorder/sdk_sources.lock.json`中driver哈希。SDK/FreeRTOS锁和校验机制未放宽。完整Host中的Recorder生成回归随后通过。
- 首次完整回归也发现无编码器旧构型出现新链接依赖；已将探针处目标源检查放入ACTIVE条件编译，最终完整Host通过。自查另修复监督故障类别映射：视觉过期仍按原视觉/帧故障OFF→ON恢复，编码器/配置/控制故障保留；不经带EXTERNAL标签的外部trip入口。新增中点无法对齐时丢弃该帧继续上一视野、精确历史端点不被后来断档误拒的回归；`metric-control`现40792条断言通过。首次失败运行`20261109T005114-19691b8278`保留，不作为验收证据。

实际C26工程隔离完整Keil构建，三个构型均**0错误、0警告**：

| 构型 | Code | RO-data | RW-data | ZI-data |
| --- | ---: | ---: | ---: | ---: |
| 默认关闭 | 89824 | 13792 | 212 | 196648 |
| 影子 | 101264 | 13792 | 212 | 199772 |
| 控制 | 102840 | 13792 | 212 | 199772 |

单位均为字节，来自各自build.log。默认关闭的大小与前轮相同，不声称BIN逐字节相同。新增常驻runtime状态在启用时占用额外3124字节ZI；这不是栈使用量或WCET测量。`audit_build.py --planner-mode`只覆盖隔离工程副本的开关，不修改主工程宏、不下载固件。

复现（仓库根目录，不访问设备）：

```powershell
python tools/testing/run_tests.py --profile Host --suite planner-runtime --compiler gcc --jobs 2
python tools/testing/run_tests.py --profile Host --compiler gcc --jobs 3
python tools/testing/run_tests.py --profile PreFlash --suite static-safety --suite project-structure --compiler gcc --jobs 2
# 仅构建副本；off/shadow/control分别使用新的输出目录。
python tools/speed_planning/audit_build.py --output .build/phase3-runtime-repro-control --uv4 E:/ElecTronic/Keil/UV4/UV4.exe --arm-bin E:/ElecTronic/Keil/ARM/ARMCLANG/bin --planner-mode control
```

冻结摘要、输入及产物哈希见`evidence.json`；精简日志和源码锁审核见`evidence/`。完整私有构建在`.build/phase3-runtime-20261109/keil-verified-*`。`changes.patch`基于本轮开始前的相关文件副本，LF归一化；共享响应checker保留了并行工作新增的turn候选兼容分支。其他Phase2/Recorder现场进展有各自记录，不由本报告补作验收。

## 阶段位置与自查

Phase3已从独立诊断链路推进到**主C26运行链路的软件集成完成、等待配置与上板验证**。仍需Phase2给出映射适用域/盲区、速度与制动、轮角/轮距、完整延迟、near/far失效恢复预算和误判依据；随后在实际构型测影子开销、PI调度抖动、输出时限、负载跟踪与故障恢复。其他构型接入、训练遥测和真实A/B/赛道验收仍有独立工作，不能称所有离线及Phase3工作全部完成。

自查：唯一writer及原C26授权未增加旁路；控制目标源和旧目标源互斥；原PI算法/生产参数不变；最终提交区无规划、几何或监督浮点计算；源码哈希对应最终Host/三种目标副本；旧回归、源码锁和静态检查保留。没有串口访问、烧录、复位或运动操作；全局配置、技能及持久Agent行为未修改。

证据层：原生C、Keil XML、JSON及日志为`structured_parsed`；编译、预处理对比、断言、CRC与哈希为`computed`。没有仅依赖`md_extracted`的关键数值，没有新增`pdf_verified`物理测量。主要准确性边界是合成输入、尚无本车配置/实时与实车结果。
