# C31 连续速度规划与视觉降级：软件阶段交付

**2026-11-09进度更新：** [Phase3最新进度](../phase3-progress-20261109/README.md)。当前处于Phase3影子上板诊断与软降级改造；V4已静态核对，near/full事件解耦已通过软件回归但未烧录。下方既往测试、板载身份和“未提交”叙述按各自时点保留；最新状态以该入口为准。

**Phase2当前口径（2026-11-09）：** 已发布[成果与交接](../phase2-calibration-20261108/DELIVERY_STATUS.md)及[当前状态](../phase2-calibration-20261108/CURRENT_STATUS.json)。已有局部落地速度/停车、左偏轮角、候选距离尺度与静止视觉记录，不再把它们统称缺测；误差、适用范围和生产边界尚待核定。转弯右轮间歇转动未解决，左右独立回调尚未实现、地面曲率未测。下方Phase3工作区说明与已发布的Phase2诊断成果分开验收。

任务来源：`SmartCar_C31连续速度规划与视觉降级协同控制_主分支改动任务书_v1.4_20261105.md`。
日期：2026-11-05。唯一主验收构型：MIDTERM_C26_31。

入口维护：2026-11-07。本阶段软件与数据现已合入main `00bb7a4`，其optimized/ASan各49 PASS / 1 SKIPPED。[主线合并与当前验证](../2026-11-07_MAIN_DOCUMENTATION_SYNC.md)优先用于当前软件状态；下列阶段报告仍保留各自时点、输入与失败记录，不改写为新运行。

**当前：Phase 0/1已完成，Phase 2数据仍未齐；Phase 3已有安全边界修正及快照核对提交。11月9日未提交增量已将共享规划器接至C26调度、原运行PI实例和唯一最终writer，完成可选运行链路的软件验证；默认关闭，待Phase2配置及上板验证，全任务尚未验收。**
最初用户没有本车标定数据；2026-11-06其他工作已交付并采用左右轮 CPR 2335.8/2358.6，详见下方当前数据接入。负载滚动周长、实际轮角、动态制动及同步运动质量数据仍缺失；参考值可用于离线设计，第 5 节 Phase 2 的生产数据与审查门尚未满足。本阶段未将新规划接入生产控制；不得用纯构建产物执行新策略的赛道验收。

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

共享算法位于 `tools/speed_planning`；最初Host交付未加入Keil。11月9日工作区已将同一内核、`demo/code/vision_planner`边界及可选C26 runtime接线加入主工程，连接原PI实例和唯一writer。主工程默认`VISION_PLANNER_RUNTIME=0/ACTIVE=0`；显式RAM配置及实际物理准入仍由调用者提供，新链路尚未上板验证。算法没有UART、串口、调试器或硬件访问接口；原控制器没有被复制成另一套长期实现。

## 仍需完成的实施

1. 建立本车几何/速度/制动标定，或按完整 session 留出验证的 ticks/s 经验模型；完成 far-loss 时距预算及恢复迟滞数据审查。
2. 主构型单一共享规划算法、调度、原PI目标源和短提交区已完成软件接线；下一步用Phase2配置进行板端影子/控制分层验证，核对真实耗时、期限与跟踪。C26授权入口及最终near-invalid清零保留。
   AUTO_TRACK与RecorderTrain后续也采用同一MPC预算接口，各自保留启动/调度边界；当前零动力恢复是预算接入前的过渡保护。
3. 视觉故障OFF→ON重新准入、严重故障不随普通OFF清除已修正并回归；共同监督/受控停止与规划器已接入可选C26链路，后续验证真实反馈及物理停止行为。仅改变行为发生变化的专项断言，其他旧回归保留。
4. 如需训练构型遥测，实现有界队列与唯一 Storage 消费者，并验证队列满/SD 延迟。现交付的是 PC sidecar 契约和解析统计，不是板端遥测写入实现。
5. 使用真实相同输入做主分支/候选 A/B、测轮速跟踪/制动/误停/圈时，再完成主构型软件与硬件分层验收。本轮未执行硬件操作。
