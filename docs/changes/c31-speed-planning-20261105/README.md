# C31 连续速度规划与视觉降级：软件阶段交付

**阅读提示：** C31是本文所述历史固件/工作阶段标识；影子模式是只计算候选、不凭候选取得真实电机权限的验证方式。规划器近场预瞄（near）与完整预瞄（full）分别判断，不能与旧控制器near/far直接等同。详见[阅读指南](../../MPC_GUIDE.md#near-full)和[术语索引](../../MPC_TERMINOLOGY.md)。本页的阶段和测试结论保留原时间；2026-11-10工作区环岛集成结果见[专项报告](../vision-scene-integration-20261110/README.md)。

**截至2026-11-09的阶段进度：** [Phase3进度](../phase3-progress-20261109/README.md)。当时处于影子上板诊断与降级改造；V4已静态核对，near/full事件解耦已通过当时的软件回归但未烧录。既往测试、板载身份和“未提交”叙述按各自时点保留；后续工作区验证见上方阅读提示。

**Phase2当前口径（2026-11-09）：** 已发布[成果与交接](../phase2-calibration-20261108/DELIVERY_STATUS.md)及[当前状态](../phase2-calibration-20261108/CURRENT_STATUS.json)。已有局部落地速度/停车、左偏轮角、候选距离尺度与静止视觉记录，不再把它们统称缺测；误差、适用范围和生产边界尚待核定。转弯右轮间歇转动未解决，左右独立回调尚未实现、地面曲率未测。下方Phase3工作区说明与已发布的Phase2诊断成果分开验收。

任务来源：`SmartCar_C31连续速度规划与视觉降级协同控制_主分支改动任务书_v1.4_20261105.md`。
日期：2026-11-05。唯一主验收构型：MIDTERM_C26_31。

入口维护：2026-11-07。本阶段软件与数据现已合入main `00bb7a4`，其optimized/ASan各49 PASS / 1 SKIPPED。[主线合并与当前验证](../2026-11-07_MAIN_DOCUMENTATION_SYNC.md)优先用于当前软件状态；下列阶段报告仍保留各自时点、输入与失败记录，不改写为新运行。

**当前：Phase 0/1已完成，Phase2部分完成；Phase3可选C26运行链路及诊断已归档至main `52b5015`，板载V4仍为影子模式。最新near/full事件解耦通过软件回归但尚未烧录；有效运动影子验证和主动动力接管尚未验收。接下来按[影子验证待做清单](../phase3-progress-20261109/README.md#待做清单推进有效运动影子验证)推进。**
已有左右轮CPR、候选距离尺度、左偏轮角、局部落地响应和停车资料，继续复用；负载滚动周长、完整地面转弯范围、动态时延与生产减速/恢复边界仍待核定。影子实验参考不作为实车生产参数，不要求重做全部Phase2后才开展有效运动影子验证。

## 阅读入口

- [2026-11-09 Phase 3：C26运行时、原PI目标源与最终writer](../phase3-runtime-20261109/README.md)：单一前台所有者、停止时配置、计数目标直接进入原运行PI；重计算移出IRQ提交区。57 PASS / 1 SKIPPED，三种隔离Keil构建0错误0警告；修复心跳测试结构问题。默认关闭，尚无板端运行/时延与实车放行证据。

- [2026-11-09 Phase 3：原生数据与米制PI诊断入口](../phase3-metric-20261109/README.md)：累计双轮计数/实际dt、capture历史与视觉快照接入影子执行器，直接计数目标复用原PI算法；1516行实测计数复算、合成闭环通过；尚未接板端调度/原运行PI实例/PWM，静态结构检查有独立失败项。
- [2026-11-09 Phase 3：监督与影子规划联动](../phase3-shadow-20261109/README.md)：轨迹状态/版本、捕获视野路程推进、发布前预算复核；3750行混合回放和重复计算优化；优先性能及稳定运行，尚未接入板端调度/PI/PWM。
- [2026-11-09 Phase 3：MCU影子接口与禁用门](../phase3-interface-20261109/README.md)：单一共享算法、米制输入/版本及有效性检查、三种输出语义、ticks/s到ticks/10ms边界；默认不运行，未接PI/PWM。
- [2026-11-09 Phase 3：C26快照与版本核对提交](../phase3-snapshot-20261109/README.md)：未提交工作区增量；主构型转向/输出及PI准备移出外围IRQ屏蔽，提交复核epoch、采样和请求generation。软件验证不等于完整规划器接入或实车验收。
- [Phase 3首批安全边界改动与验证](PHASE3_BOUNDARIES.md)：主构型近场失效最终电机清零、物理OFF→ON故障准入、严重故障保留、旧帧与UART clear绕行防护；原46 PASS/3 FAIL/1 SKIPPED保留，后续三项失配已更正，当前完整Host通过见主线记录。
- [当前数据入口与v0.3接入验证](DATA_INPUTS.md)：主文档交付70个文件哈希、9窗口/4500采样、7组手转比值复核；左右独立CPR已接入Host参考，实际米制标定与运动降级数据仍待补。
- [2026-11-06推进：参考参数与联动回放](REFERENCE_PIPELINE.md)：显式参考值、真实双PI复用、36组场景/分辨率组合、遥测满队列隔离及ARM对象证据；尚非生产控制器集成。
- [历史C0目标测试申请范围](TARGET_TEST_REQUEST.md)：本任务未执行；确定CPR的目标已由当前实测数据交付取代，不重复占用设备。
- [2026-11-06补充：C1暂定几何与编码器/舵机核对](../../reference/C1_VEHICLE_MODEL.md)：用户提供标称轮径/外形、估算轴距/轮距；CPR当前值见新交付，实际前轮角仍待测，不改变规划生产接入状态。
- [真实控制链路与现存问题](AUDIT.md)：工作区/HEAD 区别、C26 开关路径、单位、PI、Storage 和目标指令证据。
- [Host 算法与接口设计](DESIGN.md)：几何、空间约束、离散 jerk、三种输出语义、离线数据格式及使用方法。
- [缺失标定与集成门](CALIBRATION.md)：每个缺失量、候选模型、数据、验证方法和验收门槛。
- [实际验证与未完成清单](VALIDATION.md)：通过与失败、产物哈希、C01–C09 覆盖范围，不能将 Host PASS 当成正式验收。
- [机器可读证据摘要](evidence.json)：可携带报告；完整日志/私有构建位于 `.build` / `.test-results`，不纳入 Git。

## 本轮可运行内容

```powershell
# 仓库根目录；显式选用本机已有 GCC，不安装/升级工具链。
pwsh -NoProfile -File tools/testing/Invoke-TestPlan.ps1 `
  -Suite speed-planning-host,c26-baseline-characterization -Compiler gcc -Jobs 2

# 单独运行离线工具测试和 C/Python 对照；输出 .build/speed-planning/offline。
python tests/planning/test_offline.py

# 可选曲线；本机已有 matplotlib，CI 核心测试不依赖它。
python tools/speed_planning/plot_replay.py `
  --csv .build/speed-planning/offline/planning-replay.csv `
  --output .build/speed-planning/figures
```

`speed-planning-host` 是纯仿真模型检验。`c26-baseline-characterization`保留历史ID，最初用于刻画`GAP_vision_restarts_without_OFF`和`GAP_stable_OFF_clears_severe`；Phase 3修正后已更新为C26准入/清零/故障保留回归，不再期待两个缺口继续存在。历史日志和证据不改写；当前PASS仍不代表整个v1.4规划任务通过。

共享算法位于 `tools/speed_planning`；最初Host交付未加入Keil。11月9日已将同一内核、`demo/code/vision_planner`边界及可选C26 runtime接线加入主工程并归档，连接原PI实例和唯一writer。主工程默认`VISION_PLANNER_RUNTIME=0/ACTIVE=0`；隔离V4已做影子上板静态核对，但有效运动影子和主动接管尚未验收。显式RAM配置及实际物理准入仍由调用者提供；原控制器没有被复制成另一套长期实现。

## 仍需完成的实施

- [ ] **先完成有效运动影子验证。** 同帧完整原图定位并修复当前路径断点→一致影子固件/配置回归与构建→停机输入及导出核对→低速短距离运动影子。详细验收与顺序统一维护在[当前待做清单](../phase3-progress-20261109/README.md#待做清单推进有效运动影子验证)，不在旧入口另立一套前置门槛。
- [ ] **基于运动证据完成连续降级。** 推进可变路径段、残差权重、有效视野、统一能力约束与平滑恢复；动力与惩罚连续化，同因不重复扣减。当前零动力恢复及已有期限保持，普通识别波动不单独成为终止理由；STOP与必要硬边界仍优先。
- [ ] **核定受限主动接管参数并验收。** 复用已有尺度、响应与停车数据，补运行时延、所需减速能力和加速度/jerk/恢复适用边界，再验证原PI实际跟踪及停止；随后逐步扩大到弯道、环岛及相同输入A/B、误停和圈时。
- [ ] **继续完善其他构型与追溯。** AUTO_TRACK、RecorderTrain采用同一预算接口并分别验收启动/调度；按各自实现补有界记录、唯一Storage消费者及队列满/SD延迟验证。完整连续降级、最终生产定参、完整环岛回归和完整SD录像不阻塞首次影子验证，但本次记录必须可导出并对应固件、配置和运行。
