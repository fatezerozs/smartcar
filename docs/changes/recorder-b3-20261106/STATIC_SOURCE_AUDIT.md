# 主线静止测量：源码与构型审计

## 原控制owner保留

取当前用户工作区的demo/mdk/competition/midterm_switch.uvprojx为基线，保留其定义及文件集合。不是把旧Recorder训练启动器当C26主线。vision_drive.c、vision_actuator.c、vision_guard.c、vision_startup.c、vision_control.c、参数文件均直接引用原源码，没有编辑。原入口完整app_runtime_run、视觉/track处理及PIT1安全分支保留。

Keil只为下列7个文件生成私有适配副本，原路径文件不改：

| 私有文件 | 唯一目的 |
| --- | --- |
| main.c | GPT启动后启用DWT测量 |
| isr.c | PIT1原安全采样之后发布样本快照，不做串口/文件IO |
| app_mode.c | 原模式采样完成后记录C26原始/消抖状态，必要时触发已经等待的RAM采集 |
| board_encoder.c | 记录原read返回的Encoder3/4真实16-bit值，不重复清零或推断倍频 |
| vision_speed.c | PI函数前后及原PRIMASK区间做周期测量，原PI计算/提交逻辑不变，钩子在原恢复mask后发布 |
| app_runtime.c | 原UART批次后拦截静止诊断命令，仅STOP继续交给原控制；发布处理后的视觉和状态 |
| zf_driver_pwm.c | pwm_init和pwm_set_duty都把M1/M2/Servo实际duty强制0；分别记录原请求、实际传给SDK的0及VAL/MCTRL寄存器 |

这是静止、输出抑制的实验变体。逻辑请求/PI可为非零，但不能当成实际运动或生产PWM波形。原控制/PI配置中的hardware_confirmed、历史符号/servo值是既有软件字段，不是本轮重新完成的物理标定。本轮没有把截图估算值写入这些配置。

## 控制与通信差异

AUTO=0、ROUTE_SWITCH=1、ROUTE_TRIAL=1、ENCODER=1，C26仍是原生启动/重新授权owner。UART诊断不能arm车辆；@b3 arm只等待人工C26 ON触发RAM记录，不改GPIO、mode或输出权限。原生!停止和x串口所有权路径保留；主线默认owner是radio，工具先STOP再申请USB，不打开COM10。

所有其他原UART配置/探测/保存命令都被静止过滤器丢弃；不存在通过@probe/@save或UART a改变静止输出的路径。原Flash storage board init只绑定IO，不在boot擦写；不添加SD初始化或读写。MAP未链接SD_Init/SD_WriteBlocks、vTaskStartScheduler、Recorder Control/Writer或midterm_boot_poll。

## 内存与时间

事件3072×64=196608 B，单次5秒软窗口；满时停止并显式lost/reason，不覆盖。CAPTURE只重置小型状态，不在关中断区清空整个事件数组。发布单条64B事件时短暂PRIMASK保护，共享IRQ状态采用volatile；没有RTOS等待、动态分配、排序递归或ISR printf。PWM寄存器快照在短临界区读出，记录其观测值而非声称示波器波形。

最终MAP中DTCM RW_m_data使用383484/449536 B，另保留MSP8192和heap1024 B。Code92232/RO15164/RW224/ZI393500 B。ARMCLANG6.21，425输入冻结并与最终.d闭合。实际栈高水位/中断延迟/PI deadline仍待实板，不用静态预算冒充运行时证明。埋点会增加时间和代码布局开销；未来Recorder ON/OFF对照需使用同一埋点契约。

## 测试及保护

未修改断言：C26 route-switch baseline 300940 checks通过；速度PI baseline 13284 checks通过；Encoder板级stub通过；替换为私有app_mode/vision_speed钩子的同一C26测试仍300940 checks通过，PI begin/end覆盖112409次、mode覆盖74944次。全部为Host模拟，无物理速度证据。最终私有核心钩子文件SHA与该回归版本相同。

新C实现Host检查：强制零duty、STOP过滤、UART ACK消耗时间后的正确计窗、未导出保护、C26 OFF导出门、原始二进制CRC、满缓冲停止、C26触发等待/超时。PC解码8测试通过；C#客户端CheckOnly不打开COM。完整历史Host的两个既有reviewed失败没有改断言、没有用定向通过替代全套Host。

开发前保存Git状态、HEAD、原dirty patch与852项文件哈希，复核原源码/工程变化0、缺失0；仅docs/INDEX.md出现并行文档索引变化，保持其现状。输出路径唯一，不覆盖已有AXF/BIN/MAP；不修改ACL、不清理或回退Git。本轮没有执行固件下载、复位、串口通信或执行器动作。

最终补充：PI状态和last_consumed在原解锁前做固定大小快照，区外发布。Host模拟了全局speed已被更新到新样本的情况，仍断言事件绑定锁内旧样本与目标；增加该快照后的C26私有钩子回归再次运行，未修改旧断言。
