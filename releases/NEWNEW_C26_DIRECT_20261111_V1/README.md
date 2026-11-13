# NEWNEW C26 直接移植 V1

2026-11-11。按用户要求以newnew.zip为唯一算法底稿，没有混入RINGLOG或此前本车视觉/控制算法。尚未烧录和实车验收，编译通过不保证环岛通过。

## 打开与烧录

工程：C:\Users\dick\Desktop\智能车\NEWNEW_C26_DIRECT_20261111_V1\demo\mdk\newnew_c26.uvprojx
目标：nor_sdram_zf_dtcm
固件：C:\Users\dick\Desktop\智能车\NEWNEW_C26_DIRECT_20261111_V1\releases\NEWNEW_C26_DIRECT_20261111_V1\rt1064.axf
请打开newnew_c26.uvprojx；同目录rt1064.uvprojx是原始捐赠工程，未作本机设备包适配。用户手动烧录，本工具未复位、下载或发车。

## 仅做的适配

- 编码器LEFT：QTIMER2_ENCODER1，C3/C4，负号；RIGHT：QTIMER2_ENCODER2，C5/C25，正号。按板级输入选择清除QTIMER2的GPR6选择位。原始10ms读取/清零方式不变。
- M1为RIGHT：PWM C6、DIR C7；M2为LEFT：PWM C8、DIR C9。原始左PID输出接M2、右PID接M1，前进DIR=1、17kHz。
- 舵机C30、50Hz，1550us中位，1430—1670us端点。沿用原角度公式及0.5比例，角度限位换算83.7—105.3度。
- C26低电平ON可运行，高电平OFF每10ms撤销两电机并清PID积累；初始速度0，原图像主循环给定目标后才输出。C27不参与。OFF时图像处理继续，不自动重置捐赠算法内部环岛/斑马线状态。
- Keil适配本机DFP19.0.0及设备路径，原SDK保持CPU_MIMXRT1064DVL6A宏；没有替换SDK。
- 每5帧UART8 D16/D17、115200输出只读诊断NEWNEW_C26_V1，不接受串口启停。原debug UART1保留。本版未移入旧版SD/RAM记录系统。

## 原样保留

IMG_DEAL.c/.h、Threshold.c/.h、img_process.c逐字节与压缩包相同。环岛准备/入环/环内/出环状态、固定路宽表、采样行、图像阈值、路径补线、ImgDeal_GetMidError、PID计算不替换。

保留原速度目标：环岛42、其他43（每10ms编码器计数单位，不是cm/s）；PID参数P50、I0、D10.9、输出上限5000；保留TFT显示及其频率。
**保留原版斑马线识别停车；即使C26 ON也可能按原算法停车。** 本版不是此前“只关C26才停车”的RINGLOG策略。无中线时按捐赠代码返回零误差，没有新加旧版视觉租约或停车策略。

## 验证与存档

Keil0错误0警告；AXF含独立诊断身份。500个原C/H文件中仅5个板级/入口/共享变量适配文件变化，其余算法/库文件逐字节保留（实际计数以donor-audit.json为准）。PID.c仅将速度目标初始化为0并标记volatile，PID函数体不变。
完整source-code.zip、逐文件哈希、原版差异、AXF/MAP和构建日志独立保存；旧工程、旧发布不覆盖。不把其他车获奖或原算法状态值当成本车实车成功。

AXF SHA256：1A38798C2C95BBA8631DFCBB9907746C200993DE12C9C1811D7E346ED1882096
