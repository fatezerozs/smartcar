# Phase 3：共享规划器的MCU影子接口

**阅读提示：** MCU是微控制器，影子接口产生可核查的候选输出，不因计算结果授予真实电机权限；规划器和单位的中文解释见[阅读指南](../../MPC_GUIDE.md)与[术语索引](../../MPC_TERMINOLOGY.md)。本文记录当时的接口阶段。

2026-11-09（北京时间）。在[上一轮C26快照与核对提交](../phase3-snapshot-20261109/README.md)基础上，新增`demo/code/vision_planner.{h,c}`，并把接口与唯一共享内核`tools/speed_planning/speed_planner.c`加入实际主工程`MPC_20261106.uvprojx`的编译输入。

**已完成的是接口、共享源的目标编译和软件回归。接口尚未被生产调度调用，也没有接到PI或PWM。** 默认链接会剔除未引用的新接口；不能称为规划器已经在MCU运行。所有工作仍在`docs/main-documentation-20261107`的未提交工作区，不是main已合入状态。本轮未访问设备、烧录或执行运动。

## 输入、输出与禁用门

`vp_prepare`只操作调用者提供的完整输入及输出缓冲，无动态分配、时钟/设备访问、授权写入或全局控制状态。调用者提供参数，没有本车默认速度、制动、jerk、距离尺度或时距预算。

| 契约 | 本轮实现 |
| --- | --- |
| 运行模式 | 只有`VP_DISABLED`和`VP_SHADOW`。`VISION_PLANNER_ACTIVE`默认0；任意非零值会触发明确编译错误，不能靠单一宏启用未完成的生产控制路径。 |
| 有效性 | 近场、编码器、米制几何、地面适用域四项必须明确有效；未知位拒绝。该标记是上游契约，不是地面映射校验或物理真值的替代。 |
| 单位及来源 | 仅接受米制preview；距离/曲率/整形复用原内核。保留SIMULATION/PROJECT_CALIBRATION标签；标签本身不提供生产批准。 |
| 一致性 | preview必须与输入帧号及capture时间一致；绑定输出epoch、请求revision、配置revision、帧号、编码器序号、采样/capture时间和control_revision。调用者采用新轨迹状态后须推进control_revision，使旧结果无法覆盖新状态。 |
| 三种输出语义 | `ZERO_NOW`不依赖有效配置而直接清零；`TRACK`使用当前几何限速；`CONTROLLED_STOP`整形零速度目标并保留几何安全cap，不能忽略弯道/视野约束。cap无法物理满足时返回INFEASIBLE并清空候选。 |
| 近场与远场 | near-only必须配near-only几何，拒绝混入完整远场preview；近场或编码器失效立即清空候选。决定何时进入降级、受控停止或重新准入仍由外部监督层负责。 |
| 旧PI单位边界 | 输出同时携带左右ticks/s和明确的ticks/10ms整数，除100向下取整；防浮点边界上取整。量化标记保留，低速可能两轮整数均零。超出调用者提供的PI量程则整体拒绝，不单轮饱和改变比例。 |
| 发布复核 | `vp_candidate_matches`检查所有身份和帧、编码器、候选三种年龄；到期时刻拒绝，支持uint32回绕。它只允许消费诊断结果，不授予电机权限，也不检查supervisor的far/near期限。 |

失败会清空输出缓冲，防止复用上一次的非零目标；它不会修改调用者原轨迹状态。调用者仍须使用单一串行所有者，保证dt、轨迹状态、输入快照及revision对应，并在采用前执行复核。现有`vd_speed_output`接受permille请求，本轮没有把ticks直接写入该API，避免经过旧目标换算和转向混合再调制一次。

## 验证与产物

- 最终GCC optimized完整Host：`20261108T213738-51d33baf5b`，**52 PASS / 1 SKIPPED**；唯一跳过项仍为缺数据的`performance-host`。目录时间为UTC，本文日期为北京时间。
- 新`planner-interface`专项**4642次断言通过**：包含999组浮点整数边界、有效性/非有限值/单位/范围、near-only一致性、立即清零与受控停止区别、受控停止保留几何cap、全部身份失配、不同来源年龄到期和回绕。这些输入是合成数据，不是新车速或制动测量。
- `invalid-configuration`新增真实目标头文件的误开启用例，准确匹配生产编译错误。默认公共接口契约仍通过。
- 四项受控变异：移除control_revision核对、移除候选年龄核对、把计数换算除100改为除10、将受控停止目标改为非零；全部被专项拒绝。
- PreFlash仅选中`static-safety`及`project-structure`，连同environment共3节点通过。不是完整PreFlash放行。
- 实际C26主工程隔离Keil完整构建：**0错误、0警告**，明确编译`vision_planner.c`和`speed_planner.c`；Code=89824、RO-data=13792、RW-data=212、ZI-data=196648字节。主工程只增加独立源文件组；原宏、hooks及CRLF格式保留，未改base工程。
- MAP显示`vp_prepare`和`vp_candidate_matches`未被引用而剔除。与上一轮快照构建BIN比较，大小均110528字节，仅4字节不同，全部位于`main.c`既有`__TIME__`启动字符串；其余字节一致。真实文件各自哈希保留，不把归一化比较结果冒充相同SHA256。
- 本轮没有新增MSVC、ASan、远程CI或硬件通过声明。

复现（不访问设备）：

```powershell
python tools/testing/run_tests.py --profile Host --suite planner-interface --suite invalid-configuration --compiler gcc --jobs 2
python tools/testing/run_tests.py --profile Host --compiler gcc --jobs 3
```

冻结摘要、构建日志、编译剔除记录及变异检查见`evidence/`和`evidence.json`；完整Host原始记录位于`.test-results/20261108T213738-51d33baf5b`，完整构建位于`.build/phase3-interface-20261109/keil-final`。`changes.patch`只保存本轮软件内容增量（按LF归一化），以本轮开始时的工作区为基准，不包含上一轮快照和其他Phase2/Recorder工作。

## 尚待完成与自查

下一项是把既有监督状态机、真实输入快照及候选轨迹状态管理组合成受限影子执行器，再设计独立米制轮速入口复用现有双PI。不能把影子候选直接当作原permille接口输入，不能同时运行新几何分配和旧转向轮速混合。

同步路程/捕获时刻对齐、near/far有效性定义、地面映射适用域、运行时总延迟、闭环响应及停车/侧向参数还需Phase2数据审查。far预算、恢复迟滞和物理C26准入并未由本接口取代。完整速度规划/CONTROLLED_STOP与固件调度、真实PI及最终writer的共同集成和实车验收仍未完成。

自查已核对：单一算法源、无新硬件writer、禁用编译门、受控停止不越过几何cap、单位和量化、完整版本与时效绑定、测试/构建输入哈希及默认产物差异。全局配置、技能和持久Agent行为未改。

证据层：原生C、JSON、Keil XML和Markdown为`structured_parsed`；编译、回归、哈希、变异及BIN比较为`computed`。没有仅`md_extracted`关键结果或新增`pdf_verified`现场数据。准确度边界是合成输入、未运行目标代码、上游有效性及串行所有权前提，以及未批准的物理模型。
