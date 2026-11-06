# 无 Recorder 主线的时序基线监测方案

[demo/code/vision_speed.h:9](../../../demo/code/vision_speed.h#L9) 的静态阈值：采样最小10ms、最大20ms、freshness30ms、control-stall50ms。这些是程序判据，不是实测采样周期或deadline证明。
[demo/code/board_encoder.c:28](../../../demo/code/board_encoder.c#L28) 读取自由运行16-bit计数并通过模减避免read/clear丢边；每次增量需小于32768 counts。[demo/code/vision_speed.c:68](../../../demo/code/vision_speed.c#L68) 与前台control_poll分工；[demo/code/vision_actuator.c:15](../../../demo/code/vision_actuator.c#L15) 是最终权限/提交相关路径。

| 监测项 | 采集点/方法（待授权，不改生产参数） | 统计 |
| --- | --- | --- |
| Encoder间隔 | sample入口记录同源GPT时间与sample_seq | min/p50/p95/p99/max；>20/30ms次数 |
| PI服务间隔/执行时间 | vd_speed_control_poll入口/出口；DWT差分单区间小于wrap | gap、WCET观测分布、样本消耗序号 |
| PWM提交与撤销 | 最终commit/zero前后记录请求epoch、实际写入mask/LDOK状态 | target→commit延迟；OFF→zero延迟 |
| PRIMASK | 每个被审计关中断区间的入口/出口局部计时，写RAM事件环 | 屏蔽时长分布/最大；不把静态指令数称WCET |
| Camera间隔/复制 | CSI完成事件、valid frame copy、处理完成分别记录 | DMA歧义、帧间隔、copy失败、处理延迟 |
| CSI/USDHC争用 | 无Recorder基线、Camera-only、SD-only、Camera+SD分别比较 | 总线/CPU/IRQ占用、队列峰值、丢帧和控制间隔 |

不得把日志printf放进ISR或关中断临界区；事件环满只计数，不阻塞控制、不扩大权限。若使用示波器GPIO，先确认空闲引脚与复用，不擅自占用C26/UART/SD/PWM。
先审核 instrumentation 补丁及回退，再人工批准静止测量；本轮没有给主线加埋点，没有实时数据，全部实际指标为UNKNOWN。C26 OFF→ON重新授权、OFF停车及严重fault保留的原语义不变。
