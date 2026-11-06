# C1 车模暂定几何基线 v0.1 与编码器/舵机资料核对

日期：2026-11-06。来源是用户本轮提供的3张PNG、仓库既有标定记录及当前源码。用途是Host仿真和算法设计；`productionApproved=false`，不更改主构型参数。

机器可读文件：[c1-host-geometry-v0.1.json](../../profiles/c1-host-geometry-v0.1.json)。

2026-11-06后续更新：左右轮CPR现已按用户指令采用为2335.8/2358.6，前进sign−1/+1，见[当前参数交付](../changes/vehicle-parameters-20261106/README.md)和[规划v0.3数据接入](../changes/c31-speed-planning-20261105/DATA_INPUTS.md)。本页v0.1的CPR未确认/null描述保留为原时点；不覆盖新生产记录。负载滚动周长和实际前轮角仍未测，物理速度与里程仍未标定。

## 已登记参数

| 参数 | 数值 | 来源与状态 |
|---|---:|---|
| 车模长×宽×高 | 280×180×80 mm | 用户提供的产品参数原图标称，不是本车量测 |
| 轮胎直径 / 宽度 | 64 / 27 mm | 同上；未分别测左右轮 |
| 名义半径 | 32 mm | 由标称直径计算 |
| 理论周长πD | 201.0619298 mm，约201.06 mm | computed；不是负载下有效滚动周长 |
| 轴距L | 205 mm | 用户提供的暂定表，标为图像估算 |
| 后轮轮距B | 152 mm | 同上；尚未实测接地中心距 |
| 前轮轮距 | 150 mm | 同上；不等同后轮轮距 |

原图3的108.00是底板图示轮廓尺寸，不能当后轮轮距；该图没有直接标注205/152/150。后三项保留为**用户给定估算**，没有通过孔距/照片透视把它们升级为工程图认证尺寸。图中孔距、孔径、安装槽不能直接定位车轮转轴/接地点。

产品原图和暂定表逐字节留存在 [c1-model-sources](c1-model-sources/sources.json)，记录原始文件名、字节数和SHA-256。没有厂家名称、产品链接、图纸版本/公差或量具误差，测量不确定度保持null。

## 编码器文档和源码结论

依据 [ENCODER_SPEED.md](ENCODER_SPEED.md)、[board_encoder.c](../../demo/code/board_encoder.c)、[逐飞驱动](../../libraries/zf_driver/zf_driver_encoder.c)：

| 项目 | 核对结果 |
|---|---|
| 左轮 | 主板3号，C3/C4，QTMR2 channel0/1，配M2 |
| 右轮 | 主板4号，C5/C25，QTMR2 channel2/3，配M1 |
| 前进原始计数符号 | 左−1，右+1；既有现场记录及主构型配置一致 |
| 当前计数模式 | `kQTMR_PriSrcRiseEdgeSecDir`：A相上升沿/B相判向，没有启用4倍频 |
| 采样/报告单位 | PIT名义10ms；raw归一化为counts/10ms；转counts/s乘100 |
| 规格标签 | 既有用户记录称标称1024 A/B编码器；没有找到确切硬件型号/厂家规格书 |
| 轮端有效CPR | v0.1记录时未确认；现已采用左2335.8/右2358.6，见上方更新，不从1024标签推算 |

因此**不将1024或4096写成轮端CPR**。已知轮径仍不足以把现有40/132 ticks/10ms目标解释为真实m/s。

正确关系为：

\[
v_L=\frac{\Delta N_L\,sign_L}{\Delta t}\frac{C_L}{CPR_L},\quad
v_R=\frac{\Delta N_R\,sign_R}{\Delta t}\frac{C_R}{CPR_R},\qquad
RPM=\frac{counts/s}{CPR}\,60.
\]

CPR是**每个轮端一圈在当前计数模式下的有效计数**；若经轮端实测确定，不能再额外乘一次减速比/倍频。左右有效周长与CPR分别登记。工具在两者缺失时返回unavailable/抛出Uncalibrated，不能退回标称值默算物理车速。测试中的合成CPR只在临时内存副本，不写此基线。

## 舵机文档和源码结论

依据 [C26_25标定记录](../archive/versions/C26_25_SERVO_CALIBRATION.md)、[静态端点记录](../archive/history/SERVO_ENDPOINT_TEST_20261022.md) 及 [当前route profile](../../demo/code/midterm_route_profile.h)：

| 位置 | 已确认命令脉宽 | 50Hz下命令占空比 |
|---|---:|---:|
| 右侧静态端点 | 1430 µs | 7.15% |
| 中位 | 1550 µs | 7.75% |
| 左侧静态端点 | 1670 µs | 8.35% |

50Hz周期为20ms；PWM_DUTY_MAX=10000时对应715/775/835。正车体舵量向右，脉宽减小，right_sign=−1。这些是2026-10-22操作者静态卸载观察端点及本轮源码一致性核对，不是本轮重新量测，也不是实际转角/负载性能。

现有归一化舵量u经 `u(2/3+u²/3)` 整形再插值端点；Host辅助函数只复现这个**命令映射**。没有用通用“500–2500µs=0–180°”、舵机输出轴角或上述占空比换算前轮角。当前未找到舵机确切型号；即使找到舵机轴角规格，连杆/转向节的前轮角仍需实测。

`effectiveFrontAngleMap=null`。已知中位和脉宽范围不能填补左右前轮实际角、Ackermann等效角、死区、回差及加载响应数据。

## 可以继续验证的Host设计

使用暂定L=.205m、后轮距B=.152m、前轮距.150m，可以计算无侧滑理想运动学：

\[
\delta_{eff}=\arctan(L\kappa),\quad
v_L=v(1+\kappa B/2),\quad v_R=v(1-\kappa B/2).
\]

本项目κ正为右转，所以右后轮是内轮，左轮较快。这是前轮转向车辆的后轮几何补偿，没有把它改成后轮差速转向。

示例仅为**人工设计输入**：v=1m/s、右转半径1m，则理想等效前轮角约11.585°、左/右后轮速度1.076/0.924m/s。它不能反推实际舵机脉宽，不能证明本车能以这个速度/半径稳定运行，也不是生产速度建议。

```powershell
# 纯离线；输出参数状态、名义周长和上述理想运动学示例。
python tools/speed_planning/vehicle_model.py `
  --output .build/c1-geometry-20261106/model-report.json

# 已接入原有统一Host套件，无需数据集、不接触串口/硬件。
pwsh -NoProfile -File tools/testing/Invoke-TestPlan.ps1 `
  -Suite speed-planning-host -Compiler gcc
```

新增8个模型用例，合计16个Python测试，加上原C规划测试。验证暂定值/来源、理论周长、左右镜像/直线/极端曲率拒绝、计数方向/采样单位、舵机命令单调及源码一致性、原图哈希，以及缺失CPR/角度表仍拒绝转换。

本轮共享runner运行 `20261105T213230-2a2e2a4e2e`（UTC运行ID；本地日期2026-11-06），environment/compiler-environment/speed-planning-host三个节点均PASS；C检查104496次，包含100000次固定输入重复计时，不能称为104496个独立场景。[验证摘要](c1-model-sources/host-validation.json)保存profile哈希、输入层次和保留边界。仅改Host工具/测试与文档，未重跑无变更的主固件构建，也未执行硬件操作。

## 仍缺哪些生产依据

暂定几何已有数值；实际轴距/轮距、左右有效滚动周长、轮端CPR和脉宽—实际轮角仍待核实。动态抓地/加减速/jerk、相机地面标定、可信距离、总延迟、far-loss时距预算及留出session也未补齐。主构型不会因为此文件出现而自动启用新规划或转弯补偿。

读取层：三张PNG经原始图像版面核验（original_image_verified，非PDF）；既有Markdown/C源码原生读取，无仅md_extracted的尺寸结论；JSON与源码常量对照为structured_parsed，周长、运动学与测试为computed。未使用MarkItDown，输入为PNG/原生文本且该工具不可用；没有pdf_verified或新的硬件实测结论。
