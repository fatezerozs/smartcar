# Phase 0 源码与既有二进制审计

状态：整改前审计。以下位置相对工程根；此文不是最终实现验收。

## 已确认发现

| 路径 | 证据及结论 | 后续处理 |
|---|---|---|
| `vision_control.c:107`、`vc_observe()` 近远场路径 | 恢复超时仅在 recovering 时检查；near-only 路径清 recovering、更新 last_valid_ms 后继续控制 | 独立覆盖 far loss；按用户选择设计固定总零动力窗口 |
| `vision_control.c:419` 附近 | 共享 partial 路径在 C31 零需求等待分支之前，可生成 cruise 的 0.45/0.60 比例 duty | C31 近场失败先抑制电机，不能先走该非零路径 |
| `vision_actuator.c:199` | `__disable_irq()` 在浮点校验、舵机换算、混合和最终提交之前 | 输出候选计算与提交分开 |
| `vision_speed.c:259` | PI 服务在 PRIMASK 下，含目标计算和 64 位算术 | 快照/局部 PI/版本复核；不能只修改 actuator |
| `midterm_boot.c:115` 附近 | 外围关中断区调用 arm 和 output_apply | 同步收缩调用者区间 |
| `vision_speed.c:234` | 双零目标立即清 PI/目标/缓存；非零请求返回已缓存 PWM | 保留双零逻辑，补最终提交和交错验证 |
| `vision_speed.c`、`vision_actuator.c:153` | 正常 route PWM 上限独立于目标 duty；`MT_ROUTE_PWM_FULL_SCALE=1000`，最终 DRIVE 提交的物理域上限为全量 PWM | 不得把 duty=0.18 或 40 ticks 叫作最终 18% PWM 硬限制 |
| 原始与 adapted `vision_drive.c:199` | 生成器将 route fast ticks 132 替换为 recorder 的 40 | 现有 Host startup 编译原始文件，补真实 recorder 等价构型 |
| `vision_drive.c:162` | max_run_ms=0 | 如实记录总运行时限关闭 |
| `recorder_link.c`、`recorder_storage.c` | export 同时设置停止和结束请求；Storage 当前没有“等 Control 已抑制输出再进入收尾”的显式握手 | P2 加入有界握手并测试顺序，不能仅依据任务优先级认定已满足 |

既有 `output_allowed()`、`output_commit()`、autonomous_stopped 和 C31 旧启动入口禁用保护确实存在。现有代码在 PRIMASK 下保持检查到写入的顺序；本轮没有证明现有 STOP 后必然重提交非零 PWM。临界区收缩后必须重新证明，不能把版本号当成自动安全保证。

## 既有 AXF 反汇编证据

导出对象 SHA-256 为 `8fcd8c7ea83a8f9eb69614baca8987c80404118272149ad572b45aface28fe61`。

- `vd_output_apply`：地址 `0x80011b84`；`0x80011ba0` 为 CPSID i；后续存在 VCMP/VLDR 等浮点指令，多个返回点恢复 PRIMASK。
- `vd_speed_control_poll`：`0x800121d0` 为 CPSID i；路径中调用 `__aeabi_ldivmod`、`ws_pi_step_ff_ms`，并包含浮点计算。
- `midterm_boot_poll`：`0x8000aae0` 为 CPSID i；之后调用 `vd_output_arm` 和 `vd_output_apply`。
- `pwm_set_duty`：包含通道计算、LDOK 更新和 `PWM_UpdatePwmDutycycleHighAccuracy` 调用；通道边界由候选/调用者校验保证，不能把断言失败后的调试路径纳入正常提交契约。

完整与提取反汇编保存在 `.build/c31-safety-20261105-phase01/`。这些地址只适用于当前既有二进制。没有按指令数直接换算 WCET，没有测得硬件最大关中断时间。

## 中断与任务边界

| 中断/执行者 | 当前源码优先级/周期 | FreeRTOS API | 结论与风险 |
|---|---|---|---|
| PIT / 编码器采样 | IRQ 0；安全 tick 10 ms | 已检查 dispatch 与安全采样路径，无 RTOS API | 高优先级仍会被 PRIMASK 延迟；PI 在前台，不在该 ISR 运行 |
| CSI | IRQ 2 | 当前 capture IRQ 没有队列/RTOS API，使用捕获槽及寄存器 | 高于 syscall 阈值，不能随意加入 FromISR API；DMA 可继续但完成服务会被 PRIMASK 延迟 |
| USDHC1 | `BOARD_SD_Config(...,5U,...)` | SD completion 经 OSA 使用 semaphore FromISR 和 yield | 优先级数值等于逻辑 syscall 阈值；需保留该映射及 port 校验 |
| UART1 / UART8 | 未找到专用显式 priority 写入；复位默认值仅能作假设 | 当前 app_link ISR 操作自有环形队列，无 FreeRTOS API | 不能把未实测的实际 NVIC 值写成已验证；保留非阻塞 ISR budget=32 |
| SysTick / PendSV | port 设置逻辑 priority 15；tick 1000 Hz | 内核入口 | BASEPRI/PRIMASK 可推迟调度；硬件时限待测 |
| Control Task | task priority 5；每轮 delay 1 tick，超期不追赶 | 队列接收为零等待 | 高任务优先级不能抵消全局关中断 |
| Capture / Vision / Storage / Link | task priorities 4 / 3 / 2 / 1 | 按现有职责使用 RTOS | Storage 是 FatFs 唯一写入者；事件必须非阻塞 |

FreeRTOS port 为 `portable/GCC/ARM_CM7/r0p1`；临界区使用 BASEPRI，逻辑 syscall 阈值=5，内核优先级=15，priority bits=4。`interrupt_init()` 设置 priority grouping=3；本轮核对源码值及 port 约束，没有读取 NVIC 寄存器或执行 port 启动断言。

应用输出路径直接使用 PRIMASK，不能视作 taskENTER_CRITICAL 的等价实现。本轮在相关控制/recorder 源码中未找到主动设置 FAULTMASK 的路径；这不是对全部 SDK/二进制的全局无使用证明。panic/assert 的永久关中断且零输出是终止路径，应与正常短临界区区分。

## 时间预算基线与未闭合项

现有参数：相机配置 50 FPS，理论帧间隔 20 ms；Control 1 tick=1 ms；PIT/编码器名义 10 ms；frame timeout 100 ms；编码器 sample interval 允许 10—20 ms、样本 age 30 ms、control stall 50 ms。SD 单操作与 drain 有各自软件 deadline，不构成介质最大写入时延证明。

后续目标是证明：关中断区仅含有界整数快照/复核/固定通道写入，计算可抢占，拒绝旧候选；最终报告给出对应目标指令路径和预算假设。缓存 miss、Flash/SDRAM 等待、DMA 总线竞争、PWM reload 与真实 ISR 最坏延迟无本轮物理测量证据，必须保留为待测。
