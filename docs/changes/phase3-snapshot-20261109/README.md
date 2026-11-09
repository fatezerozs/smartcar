# Phase 3：C26输入快照与版本核对提交

2026-11-09（北京时间）。本轮完成不依赖最终标定值的软件子集：C26主构型在短保护区内捕获输入，在恢复调用者中断状态后准备转向/输出和双轮PI，再在提交区复核候选身份。**Phase 2、完整Phase 3和实车验收均未完成。** 工作位于`docs/main-documentation-20261107`的未提交工作区，不能表述为已经合入main。

本轮未连接串口、下载、复位或发出运动命令。Phase2响应试验使用其独立冻结候选；本轮对仓库源码的修改不代表该候选或板载固件已经更新。

## 实现与边界

| 文件 | 改动与目的 |
| --- | --- |
| `demo/code/vision_actuator.c` | 捕获控制器、profile及输出epoch；配置验证、转向和需求换算使用本地输入。仅在epoch及现时授权/故障/模式/期限复核通过后发布速度请求、方向与PWM，避免STOP后留下待执行非零请求。候选拒绝也受原epoch约束。 |
| `demo/code/vision_speed.c` | C26采用与C31同类的离线PI准备/核对提交路径；计算只读采样、PI及转向支持快照。新增请求generation，核对epoch、采样序号、请求generation、双轮请求、cap和reverse；计算期间的新采样、STOP、零请求或请求改动后又改回都会废弃旧候选。提交前重查采样年龄。 |
| `demo/code/vision_startup.c` | 去掉C26准入调用外围重复的IRQ屏蔽，由arm接口自身完成原子准入，避免首次输出准备继承外围屏蔽。 |
| `tests/planning/test_c26_snapshot.c`、`tests/suites/catalog.json` | 新增`c26-snapshot`专项，通过真实控制/PI/输出模块及模拟IO注入打断事件，并检查PWM/方向写入时的保护及调用者PRIMASK恢复。 |

控制器/profile仍由单一、串行的foreground控制所有者更新；PIT只负责采样、标记和撤权。这不是允许多个任务同时更新控制器的并发接口。测试中故意修改转向输入仅用于确认候选不会混合读取，不能据此扩展任务所有权。

原有近场无效双零、C26物理重新准入、严重故障保留和最终输出授权继续有效。未修改CPR、距离尺度、PI增益、旧转向混合、预算或生产阈值；未将Host参考策略接入主固件。

## 验证

- 最终完整GCC optimized Host：`20261108T211446-44245c7668`，**51 PASS / 1 SKIPPED**。跳过项为缺数据/基准的`performance-host`；原有auto-track、Recorder启动、motion、route/turn、速度规划和视觉回放均纳入该次运行。时间戳目录为UTC，报告日期按北京时间。
- 新专项覆盖输出准备前后及PI计算前后的STOP、物理断开、切档、撤权、采样更新、请求更改/ABA、反向请求、零请求、采样过期、数值故障和重复采样；包含已屏蔽调用者的状态恢复。计数是断言次数，不是独立现场样本数。
- 三项受控变异分别移除请求generation核对、采样序号核对，以及让PI重新读取live转向几何；均被专项拒绝。几何比较明确设置有效双边支持，并避开启动斜坡掩盖差异。
- 实际C26工程隔离Keil完整构建：**0错误、0警告**；Code=89824、RO-data=13792、RW-data=212、ZI-data=196648字节。原工程及构建宏未改，无下载动作。AXF/BIN/MAP/反汇编哈希见`evidence.json`。
- 反汇编复核：`vd_output_apply`在profile校验/转向计算前恢复PRIMASK；`vd_speed_control_poll`在PI准备前恢复PRIMASK。该证据只核对控制流程，不能换算成板端微秒数。
- MSVC专项被环境检查阻塞：本机没有可用MSVC。没有本轮MSVC、ASan或远程CI通过声明。

复现软件检查（不访问设备）：

```powershell
python tools/testing/run_tests.py --profile Host --suite c26-snapshot --compiler gcc --jobs 2
python tools/testing/run_tests.py --profile Host --compiler gcc --jobs 3
```

本轮冻结摘要与原始日志摘录在`evidence/`；测试和完整构建原始目录分别在`.test-results/20261108T211446-44245c7668`与`.build/phase3-snapshot-20261109/keil-switch`。`changes.patch`以本轮开始时的工作区为基准，保留了原有Phase2/Recorder未提交改动。

## 剩余工作与自查

这次关闭的是C26正常驱动输出准备及PI准备的长屏蔽范围。未声明所有IRQ区间均已缩短：诊断motion/probe、配置、故障处置和arm内部仍保留各自保护；短提交中仍有有界状态检查与硬件寄存器操作。尚未实测WCET、最长屏蔽时间、中断延迟或电气PWM重载时刻。

完整速度规划、jerk、米制轮速入口、far时间/距离预算和CONTROLLED_STOP与现有控制器的MCU共同集成仍待完成。下一软件项宜建立显式有效性及参数来源的规划输入/输出接口和禁用门；Phase2并行补闭环响应、实际运行时序、停车及侧向边界，审查后再启用生产策略。

自查：复核了新旧授权边界、STOP对待发布请求和PI缓存的作用、样本丢失拒绝、逆向与请求ABA、PRIMASK恢复、冻结构建与测试源码哈希。没有把Host模拟、构建或反汇编当作硬件安全验收。全局配置、技能和持久Agent行为未改。

证据层：源码、原生Markdown、JSON、Keil XML为`structured_parsed`；编译、Host回归、哈希和变异检查为`computed`。没有仅`md_extracted`关键结论或新增`pdf_verified`物理数据。主要剩余风险是实际时序未测、单任务所有权前提以及Phase2物理模型尚未批准。
