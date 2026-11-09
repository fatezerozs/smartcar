# Phase3：回到板端算法、运行链路与时序验证

2026-11-09，北京时间。用户明确停止上位机开发，任务回到Phase3。未完成的上位机四个新文件已撤回；本轮不继续开发上位机，不追加现场摆点。

## 当前流程与证据边界

| 步骤 | 状态 | 已能证明什么 |
| --- | --- | --- |
| 软件链路与Host | 已完成前轮集成，继续回归 | 单一前台所有者、原PI目标源互斥、最终writer、STOP和模式切换版本复核；不是本车物理性能验收。 |
| 板端构建/下载/身份 | V2已实机确认 | 11:01:23 Programming Done/Verify OK；用户RST后11:04:19通过COM10读取`PHASE3_C26_SHADOW_20261109_2`、加载影子配置，PWM0/0、faults0。V1历史身份/103帧结果保留。 |
| OFF影子输入 | V1已确认 | 约2秒seen增加103；near/full米制几何均0。OFF下plans=0是设计行为，不能因此判断规划器执行失败。 |
| **板端算法执行与耗时** | **三次实机通过** | `@planner_bench`每次192tick，原生监督/规划均PASS；5点合成输入下平均约36µs、本轮最大73.11µs，未开启动力或重新标定。 |
| ON影子调度与原PI共存 | 未完成实机验收 | 仍需真实授权会话、可信输入、实际反馈及执行时序；OFF和纯计算测试不替代该项。 |
| 米制目标驱动与受控停止 | 未批准启用 | 等真实输入适用域和动态参数明确后，独立检查闭环跟踪、制动、故障恢复；当前ACTIVE始终0。 |

当前真实几何适用域有限，750帧重放仅近几何36帧/完整几何3帧。原视觉valid/near/far与严格米制双边/域检查不同；没有人工真值覆盖时，不能把Host PASS或原valid标记称为图像识别正确率通过。现有三个前500mm标记共线，保留其误差和大标记中心提取限制，不强行重新拟合矩阵，不修改已加载几何配置。

## 仅用于板端纯计算的V2

身份：`PHASE3_C26_SHADOW_20261109_2`；候选配置仍为`C26-SHADOW-FA161BDE`。生成器保持RUNTIME=1、ACTIVE=0、PROFILE_REVIEWED=0，原PI增益/输出行为不变。V1项目和原始结果保留，不覆盖旧镜像。

下载入口：[MPC_Phase3_Shadow_Download.uvprojx](../../../.build/phase3-boardbench-20261109/delivery/workspace/demo/mdk/competition/MPC_Phase3_Shadow_Download.uvprojx)；纯构建入口为同目录`MPC_Phase3_Shadow.uvprojx`。

`@planner_bench`执行三个明确的合成场景：正常跟踪、短近场失效后恢复、远场预算到期受控停止。总计192个tick，每个场景64tick，预览5点，虚拟10ms步长、已知米制直线和虚拟编码器身份。仅测试副本将far预算缩为50ms以覆盖该分支；运行配置不变。

测试状态全在局部栈上，调用原`vps_init/arm/tick/result_matches`；不进入运行时实例、不调用PI、不写PWM、不授予物理输出。检查每个READY结果的身份/时限、过期拒绝和终态；正常near恢复、far受控停止是内核逻辑验证，不是真实道路或停车测量。

周期计时只包围`vps_tick`，包含监督与规划，含可能中断干扰；不含视觉/米制适配、PI、最终writer、结果复核和串口打印。5点合成输入的经验均值/最大值不是实际全链路、最坏执行时间或赛道运行认证。周期计数器由原有DWT入口启用，不复位硬件时间基准。

命令只在原字段协议先STOP且已无RUNNING/输出授权时接受；不能在运行中无意插入计算测试。它不要求先加载运行配置，可在完全停机时验证；也不清除既有冻结诊断行。Host对比检查它前后原PI、运行规划实例和冻结行均保持一致。

## 验证与下一步

专项Host首轮：`20261109T025547-ecdccb89ba`，environment/compiler/phase3-shadow-preparation均PASS。原生专项3524条断言通过；合成计时器的100周期只为测试替身，不能当作ARM实测。内核192tick中174 READY、18 ZERO、183条件检查、174过期拒绝，原PI/PWM序列对照继续通过。实际750帧图像回放的有限覆盖如实保留。

Keil完整构建0错误、0警告，Code104840、RO15520、RW212、ZI211116字节。完整Host、静态选择及后续板端结果以本目录证据追加为准，不复用旧版本的结论代替V2。

最终GCC optimized Host为`20261109T025633-a98b7b0664`：**58 PASS / 1 SKIPPED**，performance-host仍缺数据集/批准基线。静态/工程结构选择为`20261109T025633-a3e6980682`，连同environment共3 PASS；不是完整PreFlash profile。最终输入哈希与当前相关源码一致。

### 板端实测

11:01:23换烧V2 Programming Done/Verify OK；用户确认RST后，11:04:19通过COM10/115200确认身份及C26/C27 OFF、OBSERVE。静态加载影子配置后连续执行三次纯计算验证，每次3场景/192tick、174 READY、18 ZERO、183条件检查及174过期拒绝均PASS。

| 次序 | 平均周期 | 最大周期 | 按600MHz换算平均 | 换算最大 |
| --- | ---: | ---: | ---: | ---: |
| 1 | 21652 | 43866 | 36.087µs | 73.110µs |
| 2 | 21691 | 35219 | 36.152µs | 58.698µs |
| 3 | 21837 | 38511 | 36.395µs | 64.185µs |

这些是DWT及板端报告的CPU频率换算，不是外部仪器测量；仅覆盖上述5点合成场景的内核调用，不能当成全链路最坏时延。执行前后原目标和PWM均0/0、faults0，编码器累计值未变；运行规划实例仍session0/plans0，与OFF相符。诊断输出的23/23仅为局部合成候选，未交付原PI或实际输出。

第一次V2下载因Internal DLL Error失败，操作者确认退出调试/下载器就绪后成功；失败日志保留。RST后首轮控制权申请超时，后续原始串口显示STOP与x落在同一接收批次，被原协议拒绝；改为等待STOP确认及下一条BOARD_IO，再发送x后成功。没有放宽固件规则或发送运动命令，也未将这些失败写为成功。

本轮未修改地面矩阵、原PI增益、保护门限或启用米制动力。下一步仍为真实输入的影子ON会话与实际反馈/时序共存验证；不从纯计算PASS推导整车可运行。

复现：

```powershell
python tools/testing/run_tests.py --profile Host --suite phase3-shadow-preparation --compiler gcc --jobs 1
python tools/prepare_phase3_shadow.py --output .build/phase3-boardbench-rebuild --build
```

先完成停机算法实测，再解决真实米制输入接入和ON会话；不以新增上位机、反复摆点或直接放宽故障门槛代替Phase3验收。

来源层：原生C/JSON/串口字节/Keil XML为`structured_parsed`；Host断言、构建、SHA和DWT换算为`computed`。人工现场状态按用户确认记录；没有仅`md_extracted`关键数值。未修改全局配置、技能和持久Agent行为。
