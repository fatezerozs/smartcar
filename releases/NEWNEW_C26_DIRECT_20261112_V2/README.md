# NEWNEW C26 V2：1.7m/s目标与舵机抑摆

2026-11-12。基于独立V1，继续使用newnew原图像/环岛状态机，没有合入RINGLOG/MPC视觉策略。用户本轮授权修改速度与摆动控制，并指定读取main的Phase2参数集。未烧录、复位或发车，实车未验收。

## 打开工程

C:\Users\dick\Desktop\智能车\NEWNEW_C26_DIRECT_20261112_V2\demo\mdk\newnew_c26.uvprojx
目标nor_sdram_zf_dtcm。冻结固件C:\Users\dick\Desktop\智能车\NEWNEW_C26_DIRECT_20261112_V2\releases\NEWNEW_C26_DIRECT_20261112_V2\rt1064.axf。source-code.zip与本AXF对应，原V1保留。

## 速度

按照main docs/changes/phase2-calibration-20261108/DISTANCE_SCALE_CHECK.md中两次用户报告1m直推平均：左11394、右11389counts/m。控制周期10ms，1.7m/s分别对应193.698和193.613counts/10ms；两轮独立目标，未把左右尺度平均。普通道路和环岛均以1.7为非零速度目标；斑马线原有零速逻辑保留，C26 OFF或零目标立即撤销电机并清PI。

此尺度在原文仍是候选值，尚未生产放行；因此本版设置的是基于候选尺度的1.7m/s目标，不声称已测得实车1.7m/s。来源完整快照在evidence，包含Phase2成果状态及已知限制。没有改写CPR、计数方式或轮胎周长。

Phase2地面点参数Kp2、KiQ8=25为permille输出；本工程PWM满刻度10000，因此增益乘10，P20、I0.9765625，D0，原输出上限5000不变。仍调用原PidLocCtrl，修正其积分限幅/饱和时继续积累的问题，避免启用I后长期顶限。该PI在193计数目标尚未实车验证；PWM上限或电源负载可能导致无法达到目标，不通过设置更大固定PWM冒充实测速度。

## 转向抑摆

原0.5度/像素折合5.56us/像素，约21.6像素就打到1430/1670端点，帧间偏差翻转会直接反向。新比例0.22度/像素，约2.44us/像素；1像素小死区。PIT0每10ms低通alpha0.25，指令变化最多20us/10ms，2us小变化保持；每20ms发布PWM。机械右1430、中1550、左1670us、方向及50Hz不变，可达到两端点。OFF立即回中并清滤波记忆，不保留旧弯道舵量。滤波可能带来响应延迟，不保证仅凭软件能消除机械间隙或供电造成的摆动。

## 诊断与验证

UART8 D16/D17、115200，NEWNEW_C26_V2输出目标m/s、左右计数目标、候选尺度估算反馈m/s及实际舵机命令。原有每5帧输出频率保持，没有引入旧版SD/RAM记录。C26 ON运行/OFF停止，C27不参与。

Keil0错误0警告。ASAN主机：4000个舵量边界/变化率/交替小扰动断言，另有完整转角、OFF清零、1.7换算检查；PID20000饱和/积分断言及撤销/清零检查通过。这是合成检查，非1.7m/s实车追踪或防摆验收。

本版改动清单：demo/user/inc/drive_tuning.h, demo/user/inc/main.h, demo/user/src/isr.c, demo/user/src/main.c, demo/user/src/pid.c。
原图像、环岛、阈值和img_process源码与V1逐字节一致。

AXF SHA256：00BD61792B5AEB518DA602517B1B8CF0E00A58C7782D5D69F0B48541F733A20A
