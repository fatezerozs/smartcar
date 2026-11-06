# 安全、启动和实时性：软件报告

本报告把独立策略原型、原主构型现状、目标指令证据分开。PASS只对应标明的层次。

## Host 策略原型

`test_speed_planner.c`覆盖：

| 场景 | 断言 |
|---|---|
| near-valid/far-invalid | 保持授权与generation，不强制motor_inhibit；far episode起点不刷新 |
| 当前near-invalid | 当次motor_inhibit且generation变化，失效前候选拒绝 |
| near连续恢复而far缺 | 达标前持续抑制，达标后仅回DEGRADED，far时距不重置 |
| far连续恢复 | 未完成迟滞前继续episode，完成后才退出并重置 |
| 时间先到、距离先到、同时到 | CONTROLLED_STOP；多个不同encoder样本确认零速才锁止 |
| near恢复时far预算已到 | 进入CONTROLLED_STOP，不能恢复正常动力 |
| 新帧之间的预算到期 | Control周期独立检查，不依赖下一图像 |
| 延迟处理far失效首帧 | capture时刻路程对齐；采集到Control的旅行距离不能遗漏；对齐数据缺失拒绝 |
| near截止和迟到恢复同周期 | 截止先判，FAULT；不会因为恢复帧续期 |
| 重复/旧/未来帧 | 不刷新geometry/history或恢复计数，不延长frame lease |
| 相机断流 | 独立frame timeout早于near窗口终止 |
| encoder重复但改时间/路程 | 拒绝为encoder fault，不能伪造新鲜度 |
| 停不下来 | 独立stop_timeout转严重control fault；near状态变换不能重置停止意图 |
| hard STOP / 严重fault | 立即撤销，旧候选不能通过；普通frame/arm不能清锁 |
| uint32回绕 | 半域模序接受正向回绕，拒绝倒退 |

该原型只计算状态/候选资格和参考；没有在主构型实际清PI/cache/最终PWM。完整生产交错试验仍须覆盖STOP、mode/config/request版本、新样本与最后硬件提交，此轮没有冒充这项通过。

## 原主构型 C26 专项

`c26-baseline-characterization`复用原生产代码与已有模拟IO fixture，记录event/now/VC state/output_epoch/authorization/双轮target/缓存PWM/最终PWM/fault bitmap。未侵入静态PI内部，因此没有声称记录了完整积分缓存或每次真实LDOK序号。

已复现上电OFF无驱动、无有效图像时ON不准入、首次/二次OFF→ON启动、OFF撤销版本与电机零、人工STOP抑制/物理解除、UART a拒绝、严重encoder fault保持ON不恢复。

**仍存在两个v1.4目标差距：**

1. 视觉/帧故障可在ON保持时清理并重新获取5帧启动，不要求新的物理OFF→ON。
2. 稳定OBSERVE/OFF分支可清严重guard/control fault。

此专项PASS表示这两个问题也被成功复现；不是政策验收。不能删除旧断言来制造新策略通过。

## 目标构建与指令

MIDTERM_C26_31实际工程私有副本纯构建0错误0警告；宏、MAP、符号和反汇编见 [VALIDATION.md](VALIDATION.md)。主要指令地址：output_commit 0x80009e38、vd_output_apply 0x8000e250、vd_output_zero 0x8000e880、vd_route_switch_poll 0x8000e920、vd_speed_control_poll 0x8000ea64、pwm_set_duty 0x8000a4f0（限本次AXF，不是稳定ABI）。

主构型apply/PI仍有长PRIMASK及浮点；零输出函数为整数撤销，目标段可见epoch递增→清授权→vd_speed_stop→M1/M2/Servo零写→DMB→恢复PRIMASK。来源与调用者检查不能推导实际关中断微秒数、相机DMA丢帧上界或PWM物理同步。

代码与最终符号未启动Recorder任务，f_write/SD_WriteBlocks等不保留在本AXF；这支持主线没有持续SD工作流。未进行Storage压力/队列满板端试验，没有借Host格式测试声称SD实时性已验收。

## 回归边界

本轮最终专项/关联回归通过；开始前完整Host已有auto-track与recorder-startup失败，保留原始日志和断言，不处理成“测试跳过”。无MSVC/ASan执行、无远程CI、无烧录/串口/调试器/硬件操作。
