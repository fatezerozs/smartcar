# Phase3 主C26影子工程：数据更新与烧录准备

**2026-11-09进度更新：** [最新候选与上板状态](../phase3-progress-20261109/README.md)。当前处于Phase3影子上板诊断与软降级改造；V4已静态核对，near/full事件解耦已通过软件回归但未烧录。下方既往测试、板载身份和“未提交”叙述按各自时点保留；最新状态以该入口为准。

2026-11-09，北京时间。按用户确认，整理主C26影子模式：加载已有Phase2实测候选，记录米制规划结果，电机仍由原C26控制。**本轮只生成、构建和检查工程，未连接串口、烧录、复位或驱动车辆。**

## 工程入口

- **下载工程：** [MPC_Phase3_Shadow_Download.uvprojx](../../../.build/phase3-shadow-preparation-20261109/delivery/workspace/demo/mdk/competition/MPC_Phase3_Shadow_Download.uvprojx)
- **纯构建工程：** [MPC_Phase3_Shadow.uvprojx](../../../.build/phase3-shadow-preparation-20261109/delivery/workspace/demo/mdk/competition/MPC_Phase3_Shadow.uvprojx)
- **构建身份：** `PHASE3_C26_SHADOW_20261109_1`，可从`@status`的`build=`字段核对。
- **候选参数：** [phase3-c26-shadow-20261109.json](../../../profiles/phase3-c26-shadow-20261109.json)，配置ID `C26-SHADOW-FA161BDE`。

两个工程指向同一个隔离源码副本、同一Objects目录和同一个AXF，仅调试下载选项不同。没有修改主工程开关，也没有改写既有Phase2冻结工程或板载记录。`.build`是本机可重建产物，路径在其他电脑需按下面命令重建，不能用归档项目路径冒充可移植安装包。

`VISION_PLANNER_RUNTIME=1`、`VISION_PLANNER_ACTIVE=0`、`VISION_PLANNER_PROFILE_REVIEWED=0`。重启不会自动加载候选配置，也不会由新增代码授予动力。原PI增益、目标算法、PWM上限、舵机及C26准入保持原工程行为；**影子配置里的40计数换算速度、250‰字段不会限制或替换原驱动。**

## 数据已怎样更新

| 数据 | 本轮接入方式 |
| --- | --- |
| 尺寸与编码器 | 汇入已记录轴距197mm、轮距152.9mm、相机尺寸、已采用CPR与−1/+1方向；原始来源与SHA保留。 |
| 距离尺度 | 使用两次1m直推候选左11394、右11389 counts/m进行影子速度/路程计算；不修改CPR或原驱动尺度。 |
| 地面矩阵 | 采用原点更正后的后轴矩阵。预览取原图偶数像素，因此按`H原图米 = diag(.001,.001,1) × H预览毫米 × diag(.5,.5,1)`转换；没有添加半像素偏移。 |
| 采样与域 | 根据拟合距离范围生成7个内部采样行，近预览使用前5行。采样是计算结果，不是新增实测点；保留已采样地面矩形，补偿关闭。 |
| 响应与停车 | 汇入目标40的1500/2500ms落地、尺测410±20/730±10mm、末窗跟踪统计及软件零后的轮端停车尾段，同时保留三条历史滑行分析。 |
| 视觉与时序 | 保留750帧失效/恢复及静止256帧3—4ms统计，明确不是RUNNING/MPC端到端延迟。 |
| 未定参数 | 制动、jerk、侧向限制、运行延迟、动态恢复预算的最终值仍为null；为运行影子算法而使用的现有Host工程参考另列`parameterClasses`，生成C配置标记`SP_SIMULATION`，没有冒充实测或生产参数。 |

最大速度字段是原132 counts/10ms软件范围按候选尺度换算的诊断范围，并非测得最高车速。请求速度由目标40 counts/10ms换算，约0.35114m/s。连续性、零速及近远场预算仍为影子实验设置。实际PI保持原Kp1/Ki0.05输入量化值；Phase2测得的Kp2/KiQ8=25仅作为数据记录，没有在本轮换入原控制。

旧[Phase2缺项表](../phase2-calibration-20261108/PHASE2_REMAINING.md)已更新：已做的落地闭环、停车和矩阵校核不再写成未测，用户跳过的目标30不重新列为必测。真正缺测仍是运行构型完整时延、地面短弧/侧向性能和同步动态视觉里程；其余先整理已有数据，再按实际缺口验证。

## 停机读回与记录

烧录或RST前保持C26/C27均OFF、动力隔离，先确认工程身份、STOP及输出状态。新镜像沿用原主C26通信路由；先核对当前串口和owner，再发字段命令。当前Phase2最后实测镜像仍有独立[交接入口](../phase2-calibration-20261108/DELIVERY_STATUS.md)，不要把其转弯诊断命令或RAM配置直接用于本工程。

1. `@status`：核对`build=PHASE3_C26_SHADOW_20261109_1`。
2. `@planner_load`：仅在停机、编码器原配置就绪时加载影子候选，成功返回`OK`、配置ID和`mode=SHADOW`。不启动电机，不自动改PI。
3. `@planner`：查看configured、mode、source、plans、waits、seen、near_geometry、full_geometry、记录状态与DWT周期统计。`mode=1/source=0/metric_active=0`对应影子计算、旧目标源。
4. 实际ON会话仍需原C26稳定OFF→ON及有效视觉准入。所有`@`查询都按原字段协议先STOP，所以**运行中不轮询查询**；候选自身没有新增运行中UART/SD写入。
5. 停机后先`@planner`，再`@planner_row 0`至`count-1`读取。记录为256行有界环，覆盖数量由overwritten给出；STOP后冻结。未清理的冻结记录不会因新ON自动覆盖，新ON不受此诊断缓存阻塞。
6. 完成导出后才显式`@planner_clear`。该命令只清诊断记录，不授予驱动；重新加载配置前也需清空已有记录。

行中时间是板端毫秒；epoch/frame/enc/rev绑定会话、图像、编码器和规划版本；measured/planned/cap单位mm/s，ticks_R/L单位counts/10ms。flags位0/1/2/3分别为near有效、full有效、候选有效、监督会话有效。结果只是影子候选，不能作为实际已提交PWM。ASCII诊断行没有新增板端CRC；采集应保留串口原文、run/索引/冻结状态并在PC归档校验。

DWT统计分别覆盖视觉米制适配与监督/规划poll，单位CPU周期；cpu_hz来自SystemCoreClock。它们不覆盖整个采集→电气PWM链路，也不是保证WCET。记录本身、查询发送和其他系统负载的代价不合并成该统计。

## 重要的实际图像核对结果

本轮对已有750帧原始图逐帧重核CRC，再运行当前原生视觉算法与这份米制矩阵/采样域，**近几何36帧、完整几何3帧可用**。这不同于原控制记录中的750帧near有效/419帧far有效：两者有效性定义不同，米制入口还要求全部采样行双边实测并位于候选域内。

例如首帧70行中心约91像素，矩阵投影横向约−0.119m，超出当前已采样±0.1m矩形。这是本轮离线组合暴露的适配/覆盖问题，不能把此场景包装成米制全域通过；也不证明相机姿态从标定到采集完全一致。工程保留seen/near_geometry/full_geometry和规划状态，用于上板定位；这些影子拒绝不停止原控制。合成车道的接口测试通过，不能替代真实图像域覆盖。

## 复现与验证

```powershell
python -m tools.speed_planning.phase3_profile
python tools/prepare_phase3_shadow.py --output .build/phase3-shadow-rebuild --build
python tools/testing/run_tests.py --profile Host --suite phase3-shadow-preparation --compiler gcc --jobs 1
```

生成器核对输入SHA与完整候选内容后才创建新目录；构建只调用Keil重编译和fromelf，不调用下载参数。构建和下载项目的唯一预期差异是`UpdateFlashBeforeDebugging`。最终Host、静态选择、Keil日志、SHA和源码快照摘要见本目录`evidence.json`及`evidence/`。

专项包含：预览/原图及mm/m的逐点独立变换对照、篡改候选拒绝、原PI/PWM序列逐项一致、停机查询及冻结保留、越界索引拒绝，以及真实750帧域覆盖检查。主工程默认关闭、其他构型和Phase2冻结镜像保持独立；本轮不宣称MSVC/ASan、完整PreFlash、实车跟踪或生产放行。

最终GCC optimized Host：`20261109T020349-ab0a2b1ce4`，**58 PASS / 1 SKIPPED**；跳过仍为缺数据集/批准基线的performance-host。静态安全/工程结构选择：`20261109T020400-635e67c20c`，连同environment共3 PASS。候选Keil完整构建**0错误、0警告**，Code103672、RO14996、RW212、ZI211116字节。影子专项原生3498条断言通过，100组左右PWM逐项等于未加载影子时的原PI结果；750帧实际图像检查报告的是覆盖范围，不能把该专项PASS解释为全域几何合格。

证据层：源码、原生JSON/CSV、二进制帧和Keil XML为`structured_parsed`；CRC/SHA、坐标换算、回放、Host与构建为`computed`。人工尺寸/尺测仍按操作者报告注明；没有仅`md_extracted`关键数值，没有新增`pdf_verified`或物理时序证据。自查覆盖单位、坐标原点、参考值标签、单writer/旧PI隔离、构建下载分离和最终哈希；全局配置、技能及持久Agent行为未改。
