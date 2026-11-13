# NEWNEW C26 V8：单边线居中回退与贴边纠偏

2026-11-12。基线V7，原V1—V7均保留。按diagnosing-bugs流程定位软件目标偏置，新增当前边线几何处理；没有自动烧录、复位或发车。本版尚未实车验证，不能把软件测试通过当作已经消除压线或完成环岛。

## 手动烧录

工程：C:\Users\dick\Desktop\智能车\NEWNEW_C26_DIRECT_20261112_V8\demo\mdk\newnew_c26.uvprojx
目标：nor_sdram_zf_dtcm
冻结AXF：C:\Users\dick\Desktop\智能车\NEWNEW_C26_DIRECT_20261112_V8\releases\NEWNEW_C26_DIRECT_20261112_V8\rt1064.axf

C26 ON发车、OFF停止不变。普通巡航目标仍为1.0m/s；当前近端边距明显不足时，按几何余量连续限制到最低0.60m/s，居中后恢复。原避砖0.55m/s和阻塞/斑马线停车逻辑不变，没有新增停车锁或启动等待。1m/s沿用Phase2编码器比例，尚未重新认证物理车速。

## 输入证据与结论

视频37c777f4ab4264548d2e1bd5d3eb2feb.mp4，31.63秒，检查24帧，观察到连续贴边、环岛相切入口及末段弯道余量不足；联系表与校验在evidence/edge。原视频未复制进交付包。

连续COM15原始字节快照保存在evidence/edge/COM15-snapshot.bin。只分析NEWNEW V7身份，未把其他MPC身份混入：2431条完整遥测，662条ON且目标非零；其中普通道路双边居中生效45条、备用线395条，其余为环岛状态且居中层关闭。没有统一视频/串口帧号，不能断言某一日志就是视频某一瞬间。req_us/servo_us是软件需求/下发值，不是实际舵角测量。

## 红灯反馈与修正

在V7原实际Racing_Line函数的最小输入中，实际车道中心84列，应向左约10px纠偏，但固定半路宽备用线给出近端-4.38px的向右误差：

cmd /c .diagnostics\edge\one_edge.cmd（在冻结V7根运行，夹具和失败输出复制到本版evidence/edge）
ONE_EDGE actual_lane_center=84 expected_error=+10 legacy_error=-4.38 rows=21
EDGE_REGRESSION FAIL: fallback steers away from actual current lane center

修复后的实际函数对照：左边单线new_error=+10，右边镜像new_error=-10。该用例定位单边备用路径的错误目标，不是车姿随控制量变化的整车仿真；实车所有压线问题仍需后续验证。

新增不连续横跳边界负例先出现1失败，补上近端向远端连续性约束后恢复0失败，前后日志均保留。

## 改动范围

- 新增lane_geometry.h：普通稳定双边道路逐行观测宽度，至少5个一致的新图像帧后才用于单边估算；只保留路宽标量，不回放旧路径。学习仅在图像处理前后均NO_ROUND、无避障且近远方向一致时进行。
- 每帧仍必须有当前实测边、边外暗像素和通道内连续白色支持。普通道路双边中点不足时，才以单边+已观测逐行宽度补足中心；近端至少8行连续支持，逐行中心跳变不超过6px、宽度不超过8px，超过3行断口不拼接无关远端分支。路口异常宽度、暗物占据内部、无路帧不接管。
- 保留V7双边居中的优先级，新增centered=2标识备用几何居中；centered=1仍为V7双边，0仍为原算法。
- 环岛准备/入环状态0/1/2的转向指令保持原算法。沿环/出环3..7，仅当当前原选中近端路径与当前白色通道一致时，允许后置最大15us纠偏；保留原方向且偏转至少为原来的85%。原虚拟0/188入口引导不会被当作实测路段接管，不重选入口、不改环岛状态机。
- 当前边距不足时限制速度，减少持续满舵仍向路缘冲出的压力。图像列94只是现有相机坐标轴，不是地面车身投影；速度包络0.60..1.0是待实车检验参数，不能据此保证车身完全不压线。
- main每帧生成私有mask供当前边距检查，原共享Threshold和IMG_DEAL输入不变。原避障仅在前后NO_ROUND的接管门保持不变。
- 遥测身份NEWNEW_C26_V8，新增model（使用学习路宽）、near（当前近端中心误差）、margin（归一化近端中心余量）。保持连续只读记录，无运行图传、无新停车条件。

## 兼容与验证

protected-v7.json记录10个逐字节未变的文件：IMG_DEAL.c/.h、Threshold.c/.h、drive_tuning.h、brick_mask.h、obstacle_path.h、lane_centering.h、PID.c、ISR.c。main原ImgDeal_GetMidError也逐字节不变。C26、电机/编码器引脚、10msPI、V4舵机抑摆/换向、1430/1550/1670端点不改；本轮用户授权的当前边距减速和有界沿环修正例外写在AGENTS.md。

Keil构建0错误0警告。ASAN主机测试：6129项新增单边/空模型/无路/内部暗块/不连续边/虚拟入口/沿环有界纠偏/速度检查全部通过；原入口0/188真实函数、384项避砖隔离、V7居中、4000项转向与20000项PI、S弯完整换向/OFF测试继续通过。原C代码的主机编译类型转换警告保留，不冒充ARM构建警告。

可复跑命令（在工程根，需要本机MSVC/ASAN）：
cmd /c tests\run_edge.cmd
cmd /c tests\run_compat.cmd
cmd /c tests\run_centering.cmd
cmd /c tests\run_tuning.cmd
cmd /c tests\run_s-turn.cmd

## 下轮观察与未闭环项

不改变相机安装，以本版独立固件身份对照末弯和入环：观察centered=2是否在单边时触发，near是否减小，贴边时set_mps是否随margin降低，以及入环是否维持原来的方向和触发。沿环same_corridor不足时不会强改原路径，仍可能保留原来的贴边问题。

机械中位、相机横向安装/偏航、车身投影宽度、实际舵角/转弯半径、轮胎打滑、避砖完整通过和环岛出环尚未实车闭环。当前没有带位姿和延迟的动态整车模型；本次交付是针对已复现目标偏置的候选固件，不承诺保证不出界。没有修改全局路宽表、舵机增益或原环岛拓扑以掩盖问题。

AXF SHA256：8b156b79e6a3beec8dad45f08de5d032ed2bb48fe961fbad5f2a8e0a2cc2a2ae
