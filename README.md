# Smartcar：视觉识别与安全控制交付版

交付日期：2026-10-18；维护分支：`vision-debug`。保留队友原 `main` 分支和原有 Camera、Island、PID、SDK 源码。当前仅验证下述 MDK 入口，不是根目录的历史工程。

**FIELD_4 在恢复有效已保存配置后，C12 开启可延时 2 秒自动起步。烧录、调试前先断开 C12。** 没有有效保存记录时仍使用零 PWM 的未配置默认值，不会凭空得到可跑参数。电机开环、摄像头闭环转向，保留安全租约；首次配置与接线见 [C12 启动说明](docs/C12_STARTUP.md)。

## 打开哪个工程

完整解压或克隆本项目，用 Keil 打开 **[demo/mdk/rt1064.uvprojx](demo/mdk/rt1064.uvprojx)**，选择 `nor_sdram_zf_dtcm`。项目记录的编译器是 ARMCLANG 6.21，芯片 MIMXRT1064DVL6B，设备包 NXP.MIMXRT1064_DFP 19.0.0。需要自行安装 Keil/编译器和相应设备支持；它们不包含在源码包中。

在 Keil 中选择 Project → Rebuild all target files，检查 Build Output。只有成功生成新目标后才考虑原有已确认的下载流程；本次整理未烧录或驱动车辆。最新构建结果见[验证记录](docs/VERIFICATION.md)。

## 从这里阅读

- [队友接手：本次有效成果、获取方式、验证边界及修改入口](docs/TEAM_HANDOFF_20261020.md)
- [最新 FIELD_4：C12 开关、两秒启动及参数掉电保存](docs/C12_STARTUP.md)
- [当前 FIELD_3：无编码器开环 PWM、两路配平与现场反馈调参](docs/OPEN_LOOP_FIELD_3.md)
- [完整项目说明、目录和维护方法](docs/PROJECT_GUIDE.md)
- [中期现场版、停车调参与串口读取工具](docs/MIDTERM_FIELD.md)
- [贝叶斯隐状态、置信算法、训练接口与中期开环 PWM](docs/BAYES_REFACTOR.md)
- [视觉调试：串口、V2 图传、字段与排障](demo/code/VISION_README.md)
- [原版对照、相机安装标定、无编码器控制与异常管理](demo/code/VISION_CONTROL.md)
- [本次改动与保留范围](docs/CHANGELOG.md)
- [离线测试运行方法](tests/vision/README.md)

## 当前能力与限制

摄像头采集 → 自适应二值化 → 亮色通道边界/中线 → 有效性与历史检查 → 串口诊断、94×60 V2 预览。控制器采用可标定参考轴、按实测路宽归一化的视觉转向闭环和限幅开环电机占空比；FIELD_4 可恢复配置并使用 C12 本地启动，无有效记录则锁定输出。

参考场景是白色通道、较暗外侧。中期控制用于直道和左右弯道；完整赛道模型估计十字、斑马线及环岛阶段等状态，但默认未训练，完整模式不驱动车辆，尚不是环岛/十字自主导航。没有写死路线坐标、转弯时间或圈数，也没有编码器速度闭环。合成回归通过不等于实车路线验收。

首次观测：逐飞助手 V2，选择实际串口，115200 / 8N1 / 无流控，关闭自动发送及 HEX 发送；普通字符 `?` 查看帮助、`g` 看灰度、`o` 看叠线、`t` 返回文字。图像模式不同时发送文字。详细操作和限制以视觉调试说明为准。

## 源码与许可证

完整源码包含 `demo/`、`libraries/`、保留的历史工程，以及新增文档和主机测试。`Objects/`、`Listings/`、固件/主机编译产物、个人 IDE 状态不纳入版本管理；需本机重新构建。上游库与源文件的许可证声明保持原样，参见 [libraries/LICENSE](libraries/LICENSE)、各组件许可证及文件头，不将第三方代码重新声明为其他许可证。
