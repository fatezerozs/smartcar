# Phase2 连续图像与状态采集

2026-11-09。最后核对版本PHASE2_VISUAL_TRACE_20261109_1此前已烧录Keil Verify OK，COM14有线921600、OBSERVE零PWM。第四轮750帧完整校验通过，near全有效，far失效/恢复已取得；初始及移除抖动仍保留。最新已完成[实测元数据驱动Host监督回归](VISUAL_SUPERVISOR_REPLAY.md)，本轮不再追加相同现场采集，生产参数未批准。

## 为什么增加本工程

前一轮静止分段采图/文本已取得189条CAL报告，但图像与文本不同步，没有覆盖切换瞬间。控制观测无效时far输出会清除，不能直接作为独立远场支持指标。当前Recorder联合静止实证为100帧约2秒且缺少本需求的逐帧状态配对，因此本次复用当前已验证的相机拷贝/视觉/控制链，增加有限SDRAM缓存；未将Recorder验证扩大解释为本工程验证，也未改其源码。

## 同一记录包含什么

| 内容 | 来源与解释 |
| --- | --- |
| 188×120原始灰度 | 本次实际进入视觉处理的gray_frame副本，不是放大预览 |
| sequence、capture_ms | CSI收帧完成序号和GPT1毫秒时钟，不是曝光中心 |
| 处理起止、observe返回、记录拷贝起止 | 同一前台处理路径的时刻，分辨率1ms |
| raw_valid/status/quality | 原始视觉算法报告 |
| near_band_support、far_band_support | 独立调用同一control_band_error几何支持函数，不先经过全局valid/quality门槛；不是物理真值或独立near/far安全准入 |
| control_valid、control_far_valid | 当帧observe返回后的控制观测标志 |
| cue_bits、累计拷贝失败数 | 软件提示阶段及拷贝稳定性失败计数；不是人为遮挡的真实时刻 |

750帧约15秒，帧率下降时以实际capture时间为准。完整帧缓冲16,968,000字节，链接地址0x80015600，落在已有SDRAM缓存区内；不与CSI DMA共享目标内存。采集期间CPU从已稳定拷贝的gray_frame再复制图像，增加的负载需在分析中披露。源相机潜在覆盖或处理未跟上的序号间隙必须报告，不能用记录数冒充所有传感器帧。

## 现场操作（首轮只做远场遮挡/恢复）

1. 保持当前真实赛道、车和相机固定，驱动/舵机动力隔离，两拨码OFF，COM14有线；先不遮挡。
2. 操作者拿好材料，按前轮已成功方式遮远场，动作只在提示后进行。准备好回复“连续采集就绪”。
3. 我发送一次@vq_start：先正常采集，启动后约4秒第一次短鸣时放入远场遮挡；约9秒第二次短鸣时移开；采满750帧时第三次短鸣表示冻结结束。
4. 蜂鸣只是配合动作的提示。实际遮挡发生/结束的帧区间必须从连续图像另行标注，不能用4秒/9秒计划值替代人为反应和物理变化。
5. 采完后保持主控逻辑供电、不RST；冻结记录只读回传，约需数分钟。PC每50帧报告进度，完整核验750帧CRC、generation、索引、时序和序号后才标为complete。
6. 数据导出失败不清空缓存，可重新只读导出；清除需要显式@vq_clear <generation>，不自动释放。切换模式或30秒采集超时会冻结部分数据，不能冒充完整试验。

当前版本在pwm_init/pwm_set_duty两个私有入口均强制duty=0；生产PWM驱动未改，不用于落地闭环测试。USB改为921600，UART8无线仍115200，但本接口只允许USB导出。上一版115200采图脚本不能直接用于当前COM14。

## 工程与工具

- 构建工程：`C:\shengchanli\smartCar\smartcar\.build\phase2-calibration-20261108\visual-trace-20261109_1\visual_trace.uvprojx`
- 专用下载工程：`C:\shengchanli\smartCar\smartcar\.build\phase2-calibration-20261108\visual-trace-20261109_1\visual_trace_download.uvprojx`
- AXF SHA256：`1fb032b58c3deecb111fab11151d3544985e3929cd1138ed625e40c567fdc3d8`

PC工具tools/speed_planning/visual_trace.py：--check只核对身份；--start启动空缓存、等待冻结并导出；无--start时只导出已有完整缓存。所有模式先STOP/取得USB所有权/核对专用版本和OBSERVE，不发arm或运动命令，不恢复RAM运动参数，不自动清除数据。

每帧32字节头（8个小端uint32：VQF1/generation/index/size/CRC32/188/120/16），随后22624字节载荷：16个小端uint32元数据加22560字节原图。CRC32覆盖完整元数据和像素；PC另保存文件SHA256。冻结期间通过直接UART阻塞回读，采集期间不导出，故回读阻塞不应混入采集时延。每个原始.vqf均保留，以便复核与重新解析。

## 验证与边界

最终Keil 0错误、0警告，Code75408、RO14660、RW212、ZI17128472字节。首次编译缺app_link.h导致的APP_LINK_USB未声明已修正。链接map已核对大缓存位于SDRAM，两个PWM入口仍强制零。

Host检查覆盖OBSERVE/USB/空缓存启动门槛、拒绝启动前旧帧、容量和提示阶段、冻结后保留、严格索引、generation清除、uint32回绕超时、模式中断和二进制导出。Native C导出的完整帧由Python解析器验证CRC一致；像素篡改、错误generation/索引、截断、有效CRC但越界quality均被拒绝。模拟元数据还核对了控制无效时独立far几何支持可保留为1，但这不等于目标板视觉行为已验证。

第二轮750帧完整CRC/SHA256及冻结信息复核通过，源序号缺口0、拷贝失败0；但749帧控制观测无效、仅1帧有效，无遮挡基线不稳定，远场单独失效/恢复场景未通过。详见[第二轮结果](VISUAL_CONTINUOUS_RESULT_02.md)。生产参数未批准。当前仍是静止OFF分支，不能证明RUNNING/MPC耗时、预算降级、物理曝光/PWM延迟、速度模型或安全制动边界。

证据层：C源码直接核查；项目XML、JSON、串口身份为structured_parsed；构建、链接、测试、CRC及哈希为computed。无仅md_extracted或新增pdf_verified结论。


**2026-11-09第三轮更新：** 第三轮750帧完整校验通过，全部近远有效，正常基线恢复；板材遮挡未触发far失效，场景验收仍未通过。首末累计拷贝失败均2、增量0。下一步先实时核对有效远场遮挡摆放，再采完整切换。见[第三轮报告](VISUAL_CONTINUOUS_RESULT_03.md)。


**2026-11-09第四轮更新：** 第四轮750帧完整校验通过，近场全程有效；far有效419帧、无效331帧，其中连续失效210帧、末段恢复167帧。完整功能切换已记录，初始及移除波动保留，干净受控场景和生产预算仍未通过。见[第四轮核定报告](VISUAL_CONTINUOUS_RESULT_04.md)。下一步进入离线回归分析，不追加相同现场操作。
