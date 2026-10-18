# Smartcar：视觉识别与安全控制交付版

交付日期：2026-10-18；维护分支：`vision-debug`。保留队友原 `main` 分支和原有 Camera、Island、PID、SDK 源码。当前仅验证下述 MDK 入口，不是根目录的历史工程。

**默认是静态视觉观测程序，不会自行跑车。** `TEST_VISION=1`，`VISION_DRIVE_ENABLE=0`，相机/硬件标定未确认，电机占空比和舵机脉宽未配置。不要仅打开 enable，也不要为了跑车切回依赖故障编码器的旧整车模式。

## 打开哪个工程

完整解压或克隆本项目，用 Keil 打开 **[demo/mdk/rt1064.uvprojx](demo/mdk/rt1064.uvprojx)**，选择 `nor_sdram_zf_dtcm`。项目记录的编译器是 ARMCLANG 6.21，芯片 MIMXRT1064DVL6B，设备包 NXP.MIMXRT1064_DFP 19.0.0。需要自行安装 Keil/编译器和相应设备支持；它们不包含在源码包中。

在 Keil 中选择 Project → Rebuild all target files，检查 Build Output。只有成功生成新目标后才考虑原有已确认的下载流程；本次整理未烧录或驱动车辆。最新构建结果见[验证记录](docs/VERIFICATION.md)。

## 从这里阅读

- [完整项目说明、目录和维护方法](docs/PROJECT_GUIDE.md)
- [视觉调试：串口、V2 图传、字段与排障](demo/code/VISION_README.md)
- [原版对照、相机安装标定、无编码器控制与异常管理](demo/code/VISION_CONTROL.md)
- [本次改动与保留范围](docs/CHANGELOG.md)
- [离线测试运行方法](tests/vision/README.md)

## 当前能力与限制

摄像头采集 → 自适应二值化 → 亮色通道边界/中线 → 有效性与历史检查 → 串口诊断、94×60 V2 预览。控制扩展采用可标定参考轴、按实测路宽归一化的视觉转向闭环和限幅开环电机占空比，默认关闭。

参考场景是白色通道、灰度较暗的外侧，包含直道和左右弯道；没有写死赛道坐标、转弯时间、圈数或截图标定值。不是任意道路语义识别、环岛/十字导航，也不是编码器失效情况下的速度闭环。合成回归通过不等于实车路线验收。

首次观测：逐飞助手 V2，选择实际串口，115200 / 8N1 / 无流控，关闭自动发送及 HEX 发送；普通字符 `?` 查看帮助、`g` 看灰度、`o` 看叠线、`t` 返回文字。图像模式不同时发送文字。详细操作和限制以视觉调试说明为准。

## 源码与许可证

完整源码包含 `demo/`、`libraries/`、保留的历史工程，以及新增文档和主机测试。`Objects/`、`Listings/`、固件/主机编译产物、个人 IDE 状态不纳入版本管理；需本机重新构建。上游库与源文件的许可证声明保持原样，参见 [libraries/LICENSE](libraries/LICENSE)、各组件许可证及文件头，不将第三方代码重新声明为其他许可证。
