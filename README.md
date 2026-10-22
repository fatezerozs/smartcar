# Smartcar：视觉识别与安全控制交付版

> **2026-10-22 最新交付：MIDTERM_C26_19。** C26 单开关连续循迹、当前白路及居中控制、弯道/直道提速、出十字单边恢复、最右圆岛进出与横向裁切识别。先读[本次有效成果与交接](docs/TEAM_HANDOFF_20261022.md)。Keil 使用 [demo/mdk/midterm_switch.uvprojx](demo/mdk/midterm_switch.uvprojx)，C26 ON 稳定看到赛道后自动启动，OFF 停车。固件和校验在 `releases/`；软件测试已通过，C19 实车完整环岛和整圈尚未验收。以下较早条目是历史版本的操作和验证状态。

> 2026-10-21 本次现场更新：已使用 MDK Community / ACfE 6.24 完成训练版构建和烧录校验，并通过串口确认 `MIDTERM_TRAIN_1`。基础参数在停车状态恢复至 RAM，新增逐条确认串口所有权的恢复工具。当前相机未标定、cruise=0，尚未进行动态循迹试跑，C12 不负责启动。详见[当天现场记录](docs/FIELD_20261021.md)。

> 2026-10-21：静态相机参考轴与左右平移/航向偏差方向检查完成，参数与后续实体纠偏步骤见[中期第一阶段](docs/MIDTERM_STAGE1.md)。标定仅RAM，自主视觉确认位仍锁定。

> **最新速度闭环基线：ENCODER_SPEED_DIAG_5。** 编码器3左/4右，直线、主动/失联停车、往复倒退与左右短圆弧已由操作者验证；速度参数和最新舵机左右映射见 [编码器与速度环](docs/ENCODER_SPEED.md)。中期训练/自主源码一并交付，自主视觉循迹尚未验收，确认位保持锁定。

> 新增中期训练/自主版本：上电 3 秒一次性启动、速度环与视觉联调及分阶段验证见 [中期实施与验证](docs/MIDTERM_IMPLEMENTATION.md)。当前自主配置仍锁定，必须先完成真实速度参数填写、参考轴标定与视觉纠偏方向核对。

> **2026-10-20 实车测试基线：MOTION_DIAG_2。** 无线双串口、两轮/舵机点动、直线、停车确认后倒退、左右短圆弧和 `!` 动态停止已由操作者验证。先读 [基线与复现入口](docs/BASELINE_20261020.md) 和 [运动诊断操作](docs/MOTION_DIAG.md)。正式固件默认限制与独立诊断固件不同，不能混用下载产物。

> 当前四键控制已回退，C12 不再参与启停；已确认有效的 B11 修正保留。上电保持停止，拨码仍选档。无线接管能力与边界见 [回退及无线评估](docs/WIRELESS_CONTROL_ASSESSMENT.md)。

当前维护分支：`main`（已同步 vision-debug）。本轮按功能拆分运行调度与驱动模块，并接入 C26/C27 两位拨码。原有 Camera、Island、PID、SDK 源码保留。

**UART_CONTROL_BASE_V1：上电和恢复配置后保持停止，四键不参与控制。** 使用当前串口控制端显式启停，标定、图像有效性、指令心跳和故障门槛仍保留。

新固件的两位拨码全 OFF 为纯观测；其余档位为舵机点动、电机点动和整车控制。点动档不依赖摄像头。详见 [架构与拨码操作](docs/ARCHITECTURE_MODES.md) 和 [B11 蜂鸣器排查](docs/BUZZER_DIAGNOSIS.md)。最新诊断固件已由用户烧录并完成上述实车测试，离线检查与实测边界见 [验证记录](docs/VERIFICATION.md)。

## 打开哪个工程

完整解压或克隆本项目，正式入口为 **[demo/mdk/rt1064.uvprojx](demo/mdk/rt1064.uvprojx)**，目标 `nor_sdram_zf_dtcm`。复现实车基线需先按 [构建说明](docs/BASELINE_20261020.md) 生成独立 `motion_diag.uvprojx`。项目记录的编译器是 ARMCLANG 6.21，芯片 MIMXRT1064DVL6B，当前设备包 NXP.MIMXRT1064_DFP 26.09.00。Keil、编译器和设备包不包含在源码库中。

在 Keil 中选择 Project → Rebuild all target files，检查 Build Output。只有成功生成新目标后才考虑原有已确认的下载流程；本次整理未烧录或驱动车辆。最新构建结果见[验证记录](docs/VERIFICATION.md)。

## 从这里阅读

- [编码器3左/4右、1024正交计数与速度环（新增，待上板标定）](docs/ENCODER_SPEED.md)
- [本次实车成果、外设接线、证据和复现](docs/BASELINE_20261020.md)
- [已验证运动诊断版：配置、直线、往复、圆弧和动态停车](docs/MOTION_DIAG.md)
- [最小运动测试设计及原正式版本限制](docs/MINIMAL_MOTION.md)
- [双串口：最小回显验证、无线接管与中断调整](docs/DUAL_UART.md)
- [队友接手：本次有效成果、获取方式、验证边界及修改入口](docs/TEAM_HANDOFF_20261020.md)
- [历史 FIELD_4：C12 启动设计，当前已停用](docs/C12_STARTUP.md)
- [历史 FIELD_3：无编码器开环 PWM与两路配平](docs/OPEN_LOOP_FIELD_3.md)
- [完整项目说明、目录和维护方法](docs/PROJECT_GUIDE.md)
- [中期现场版、停车调参与串口读取工具](docs/MIDTERM_FIELD.md)
- [贝叶斯隐状态、置信算法、训练接口与中期开环 PWM](docs/BAYES_REFACTOR.md)
- [视觉调试：串口、V2 图传、字段与排障](demo/code/VISION_README.md)
- [原版对照、相机安装标定、无编码器控制与异常管理](demo/code/VISION_CONTROL.md)
- [本次改动与保留范围](docs/CHANGELOG.md)
- [离线测试运行方法](tests/vision/README.md)

## 当前能力与限制

摄像头采集 → 自适应二值化 → 亮色通道边界/中线 → 有效性与历史检查 → 串口诊断、94×60 V2 预览。视觉驾驶保留参考轴、图像和人工解锁门槛；独立运动诊断不依赖道路识别。上电、恢复配置或切档都不自动发车，C12和四键不再参与启停；只有显式命令才请求动作。

参考场景是白色通道、较暗外侧。中期控制用于直道和左右弯道；完整赛道模型估计十字、斑马线及环岛阶段等状态，但默认未训练，完整模式不驱动车辆，尚不是环岛/十字自主导航。没有写死路线坐标、转弯时间或圈数，也没有编码器速度闭环。合成回归通过不等于实车路线验收。

首次观测：逐飞助手 V2，选择实际串口，115200 / 8N1 / 无流控，关闭自动发送及 HEX 发送；普通字符 `?` 查看帮助、`g` 看灰度、`o` 看叠线、`t` 返回文字。图像模式不同时发送文字。详细操作和限制以视觉调试说明为准。

## 源码与许可证

完整源码包含 `demo/`、`libraries/`、保留的历史工程，以及新增文档和主机测试。`Objects/`、`Listings/`、固件/主机编译产物、个人 IDE 状态不纳入版本管理；需本机重新构建。上游库与源文件的许可证声明保持原样，参见 [libraries/LICENSE](libraries/LICENSE)、各组件许可证及文件头，不将第三方代码重新声明为其他许可证。
