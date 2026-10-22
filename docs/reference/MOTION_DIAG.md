> 当前适用范围（2026-10-23）：独立运动诊断变体；不是C31循迹操作。当前主程序、参数和完成状态统一见 [项目设计与演进档案](../PROJECT_EVOLUTION.md)。

# 已实测硬件的运动诊断入口

本侧对话已由用户现场观察确认：M1为右轮、DIR=1向前，150‰/1500ms可转动；
M2为左轮、DIR=1向前，100‰/1500ms可转动。舵机约1550µs回中，1500偏左、
1600偏右，两端未观察到卡滞，因此本安装right_sign=+1。
这些是观察到的工作点和保守窗口，不是最低起转阈值、两轮等速证明或完整机械极限。

## 固件

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_motor_threshold.ps1 -Variant motion
```

独立工程：`demo/mdk/motion_diag.uvprojx`。
只下载 `.build/motion-diag/Objects/rt1064.axf`，不要选择Example的产物或原Objects。
复位后必须看到 `build=MOTION_DIAG_2`；下载日志的Load路径须与上面一致。
本脚本仅构建，不下载、不打开COM、不启动电机。
默认正式版本保持100‰整车上限；此诊断版整车请求最大150‰，首条允许150‰。
仍须显式配置和发送动作命令，保留单轮诊断、本地自动启动禁用、PIT1停车和双串口仲裁。
摄像头中断、安全中断和运动斜坡速率均未改变。

## 停车配置（每次复位后重新设置，不发@save）

先架空轮子，关闭自动发送。串口115200/8N1，HEX发送关闭。
发送 `!`，切DRIVE档，用`t`确认原始C26=0、C27=0、mode=DRIVE、ready=1。
按次序逐条发送，每条@命令附换行，等待OK再发下一条：

```text
x
@servo 1550 1500 1600 1
@motor_dir 1 1
@balance 1 0.667
@motor 0 0.15 0.1
@status
```

预期：[MOTOR] confirmed=1 dir=1/1 scale=1.0000/0.6670，servo=1550/1500/1600，
sign=1，cruise=0，cap=0.15。cam=0和视觉ready=0不阻止独立运动测试；不要伪造相机标定。
这里0.667使公共150‰时M1实际150‰、M2经整数缩放实际100‰；这不是等速配平。
原视觉巡航保持0；不要发送a。

## 首轮：限时直线和动态停车

配置确认后，在拥有控制权的同一串口每100ms只重复发送单字符 `h`。
动作命令必须手动单发一次，绝不能设成自动发送：

```text
@motion line 150 2500
```

先200ms零电机PWM预置舵机，再逐步增加PWM。运动段约1500ms达到右15%/左10%，
2500ms到时全部撤PWM；总时长约2700ms。先架空确认两轮向前和自动停止，再在空旷地面
短时观察是否偏航、拖轮。不要把不同占空比当成直线保证，也不要靠加长时间弥补不转。

每次结束停止发送h，确认机械停止，至少再等2秒。开始新动作时先重新启用h。
第二次相同命令可验证显式停车：等两轮确实转起来，在约开始后2秒发送单字符 `!`，
应看到phase=0、reason=2，轮子撤驱动且不会自行恢复。第三次可仅关闭h自动发送，
最后心跳满500ms后应phase=0、reason=3；恢复h不会重新启动。
不要在运动中发@status，它本身也会触发停车，导致混淆停机原因。
先架空完成这两项，再落地检验实际运动中的停车；PWM=0不等于电气制动或立即机械静止。

## 后续：一次往复与短圆弧

直线和停车确认后，保持同样配置，恢复每100ms的h，单发：

```text
@motion shuttle 150 2500
```

前进段结束为phase=3。持续h，确认机械完全停止且等待至少2秒，再单发 `v`，
随后零PWM预置200ms并倒退2500ms后结束。此时DIR均为0，倒车是首次实车验证，
先架空观察。未发v不倒车，等待超过8秒会结束；不保证前后行程相等。

短圆弧也必须先停稳、至少间隔2秒，保持h，再选一条单发：

```text
@motion circle 150 2500 1500
@motion circle 150 2500 1600
```

分别是左偏、右偏舵角；每次只发一条并观察。它是限时圆弧，不能保证整圈或固定半径。
随时可从任一串口发送 `!` 停止。

## 可选主机助手

完成RAM配置、关闭逐飞助手对所用COM的连接后，可使用现有助手。
不加Execute只预览，不打开串口；以下也不执行实际运动：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/motion_test.ps1 -Mode line -DutyPermille 150 -DurationMs 2500
powershell -NoProfile -ExecutionPolicy Bypass -File tools/motion_test.ps1 -Mode line -DutyPermille 150 -DurationMs 2500 -StopAfterMs 2000
powershell -NoProfile -ExecutionPolicy Bypass -File tools/motion_test.ps1 -Mode line -DutyPermille 150 -DurationMs 2500 -DropHeartbeatAfterMs 1900
```

要实车执行需另加 `-Port COM10 -Role radio -Execute`（无线）或实际有线COM与`-Role usb`。
助手只维持h并单次请求动作，不替用户设置标定；退出尽力发送STOP。首次建议逐步手动观察。

## 离线验证

`tests/vision/run_motion_tests.cmd optimized diag`及`asan diag`运行真实生产代码、模拟IO，
核对150/100‰输出、左右脉宽、单次往复、零PWM换向、截止时间、主动STOP、失联与切档停车。
不加diag的原版测试继续核对正式限制。离线测试不能证明实车走直、停车距离或圆周轨迹。

## DIAG_2输出更新修正

用户现场确认DIAG_1在脚本有效心跳下达到软件actual=150、最终reason=1，但实体不转。
因此之前的心跳超时只解释了较早的一次失败，不能解释这次结果。
代码检查发现：运动路径每次poll/视觉回调都重写三路PWM，逐飞驱动每次执行
CLDOK→写缓冲值→LDOK，并配置为完整PWM周期装载。
DIAG_2只在某一路目标值改变时重写该路PWM，保持期间不重写，电机爬升也不反复重载50Hz舵机。
任何停车仍直接写三路零并使缓存失效，下一次显式启动重新下发。
PWM上限、斜坡、配平、心跳、相机/安全中断和停车条件不变。
新增回归检查保持期不重复写PWM、斜坡不重写舵机、停车后可重新下发同样数值。
该修正针对已发现的重复写入问题；它是否解决实体不转仍需重新烧录后观察，不能以软件actual冒充波形。
