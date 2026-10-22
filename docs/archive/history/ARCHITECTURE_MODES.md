> 历史归档：本文只描述原版本，不作为当前启动、参数或验收说明。当前现状见 [项目设计与演进档案](../../PROJECT_EVOLUTION.md)。

> 当前状态：四键控制已回退，C12 不再参与启停，B11 修正和拨码选档保留。本文按键行为为历史记录，当前以 [回退及无线评估](WIRELESS_CONTROL_ASSESSMENT.md) 为准。

> 当前按键行为与读回方法已更新，请先看 [IO_KEYS_V2](KEYS_AND_GPIO_DIAGNOSTICS.md)。旧版“C12 松开停止”仅为历史说明。

# main 架构与拨码模式

本页描述当前main架构。正式入口为`demo/mdk/base/rt1064.uvprojx`，根目录和IAR工程为历史保留。独立MOTION_DIAG_2已完成无线及基础运动实测；版本差异和证据见 [当前基线](BASELINE_20261020.md)。

## 模块职责

| 模块 | 唯一主要职责 |
| --- | --- |
| `demo/user/src/main.c` | 上电初始化和进入应用 |
| `board_io.c` | B11 蜂鸣器电平、C26/C27 输入及采样 |
| `board_camera.c` | 按需初始化相机及失败重试节流，不含重试等待循环 |
| `app_mode.c` | 两位输入解码、消抖、模式权限和变化事件 |
| `app_runtime.c` | 前台协调：命令、帧、视觉、控制与展示调用 |
| `app_link.c / app_link_board.c` | 双UART独立队列、非阻塞硬件服务和有界RX中断 |
| `app_transport.c` | 控制权、命令来源隔离、停止优先和格式化 |
| `vision_frame.c` | 相机缓冲区复制及一致性检查 |
| `vision_telemetry.c` | 帮助、诊断报文和 V2 预览 |
| `vision_midterm.c` | 通道视觉算法，无执行器 IO |
| `track_bayes.c` | 赛道状态观测，仍不驱动完整赛道 |
| `vision_control.c` | 视觉控制算法和 DISARMED/RUNNING/FAULT 状态 |
| `midterm_open_loop.c` | 电机占空比模型，无轮速反馈 |
| `vision_drive.c` | 驱动生命周期及命令协调门面 |
| `vision_vehicle_profile.c` | 车辆配置校验、查询和提交 |
| `vision_actuator.c` | PWM 映射和输出仲裁 |
| `vision_guard.c` | PIT1租约、点动/运动期限与拨码变化撤输出 |
| `vision_startup.c` | 保留历史开关兼容核心，当前入口不调度自动启动 |
| `vision_motion.c` | 定舵角运动、缓升、人工确认倒退与各路PWM保持更新 |
| `vision_probe.c` | 有界舵机/单电机点动 |
| `vision_field.c` | 现场命令解析 |
| `vision_storage*.c` | 记录格式、CRC、双槽及板端 Flash 适配 |

`vision_drive_internal.h` 中的私有上下文仅供驱动实现文件共享；它保留前台/ISR 原有临界区和唯一输出仲裁。不要让应用、视觉或命令解析器直接修改该上下文。`vision_debug_run()` 只保留兼容调用，实际调度位于 `app_runtime_run()`。

## 两位拨码操作

用户提供的原理图表明 S1 是两位拨码，不是旋转编码器：C26/C27 断开由上拉得到高电平，ON 接地得到低电平。按**引脚**定义下表，不根据照片猜测开关外壳的 1/2 顺序。

| C26 | C27 | 模式 | 允许动作 |
| --- | --- | --- | --- |
| OFF / 高 | OFF / 高 | OBSERVE | 摄像头观测、串口诊断；禁止执行器动作 |
| ON / 低 | OFF / 高 | SERVO | 只开放 `@probe` 有界舵机点动 |
| OFF / 高 | ON / 低 | MOTOR | 只开放 `@wheel` 单轮有界点动 |
| ON / 低 | ON / 低 | DRIVE | 视觉驾驶需标定和有效图像；独立运动测试需执行器标定及心跳 |

- 四档使用同一份固件，无需修改 `TEST_SERVO/TEST_MOTOR/TEST_UART` 后重新编译。
- 当前不初始化/读取C12～C15，四键及C12主使能已停用。切档不直接启动任何执行器。
- 舵机、电机档不调用相机初始化，不必连接摄像头即可发点动命令。切到观测/整车档后才按需尝试初始化，失败不在初始化函数外循环等待。
- 原始拨码变化在下一次 PIT1 采样（名义间隔 10ms）时禁止运动并撤输出；稳定 30ms 后接受新档。前台清旧视觉证据、停止运行，跨档分段命令丢弃至行尾。不能声称机械上瞬间停稳。
- 切回DRIVE或恢复保存配置均不恢复旧运行。视觉驾驶需新鲜图像及显式`a`；独立运动需显式`@motion`。两者互斥，远程心跳不能恢复已结束动作。
- 点动仍保留原有时长、步长、静默期、零巡航/上限配置要求；本次没有改成自动反复扫舵机或轮流转电机。
- 单字节 `g/b/o/t` 仍负责图像展示，`f/m` 仍负责观测算法选择；它们不能越过硬件档位授权运动。

`VISION_DRIVE_ENABLE` 是固件裁剪开关；串口类型、硬件替身和合成标定宏仍用于主机编译矩阵。它们不是实车测试档位，也不应被拨码替代为“自动确认硬件”。主机模拟电平调用与板端相同的 `app_mode_sample`，在一个测试程序中覆盖所有档位。

## 验证入口

`tests/vision/run_tests.cmd` 和 `run_tests.cmd asan` 链接所有拆分模块。`test_board_mode.c` 检查 GPIO 初始化顺序、蜂鸣器极性、相机按需重试、四档权限、全部档位转换、30ms 边界、时钟回绕、点动/行驶时原始切档撤输出和禁止旧运行恢复。`test_debug.c` 的 USB/无线场景另检查实际主循环切档、禁止观测档解锁、跨档分段命令和原有图传/控制行为。

工程引用检查仍为 `tools/verify_project.ps1`。`tools/check_arm.ps1` 是单源 ARM 编译，`tools/build_keil.ps1` 才是完整重建；均不烧录。主机通过不能替代 ARM 链接、上电 B11 波形、实际拨码线序及实车控制验收。
