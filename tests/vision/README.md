# 可复现的主机验证

FIELD_4 新增 test_startup：C12 两秒启动、原始断开撤输出、无抖动重启、无效到期不补启动、保留远程心跳/本地视觉及总时限、CRC 双槽/写断电/单比特破坏/序号回绕。USB/无线适配新增真实主循环恢复配置后启动、STOP/图传取消、开关停止及无效后不重试场景。旧子系统夹具通过 HOST TEST 编译参数关闭物理开关门槛；test_startup 和部署目标保持开关启用。板级 Flash 适配仅 ARM/完整链接检查，尚未实机写入。

这些测试在电脑上编译 C 源码，模拟相机、计时器、串口和 PWM，不连接小车、不烧录、不输出真实 PWM。合成通过不是实车安全或路线验收。

Windows 需要 Visual Studio 2022 C++ Build Tools/MSVC（含 C11 支持），ASAN 运行另需安装 MSVC AddressSanitizer。脚本优先使用已在 PATH 的 cl，否则通过 vswhere 查找 x64 工具环境，不依赖个人用户名。PowerShell 脚本推荐 PowerShell 7。

从仓库根目录运行：

```powershell
cmd /c tests\vision\run_tests.cmd
cmd /c "tests\vision\run_tests.cmd asan"
cmd /c tests\vision\run_equivalence.cmd
cmd /c "tests\vision\run_equivalence.cmd asan"
& .\tools\verify_project.ps1
& .\tools\check_arm.ps1
python tests/vision/test_fit_model.py
# 完整重建；需要本机 Keil 和设备包，不下载固件：
& .\tools\build_keil.ps1
```

ARM 默认安装路径为 `C:\Keil_v5\ARM\ARMCLANG\bin`，可通过 check_arm.ps1 的 `-ArmBin` 修改；Keil GUI 程序默认 `C:\Keil_v5\UV4\UV4.exe`，可通过 build_keil.ps1 的 `-UV4` 修改。脚本失败会返回非零状态，请查看输出，不要将单文件编译成功当作链接成功。

run_tests 默认使用 `/O2 /W4 /WX`，asan 使用 `/Od /Zi /fsanitize=address /W4 /WX`。应顺序运行，不能同时写同一个 output 目录。产物在 tests/vision/output，ARM 检查和 Keil 批量日志在 .build，均不提交。

| 文件 | 检查范围 |
| --- | --- |
| test_vision.c | 合成直道、偏移、弯道、遮挡、曝光/阴影、歧义、补线过期、随机图、边界连续性和预览像素 |
| test_debug.c + stubs/main.h | 静态 USB、模拟确认配置 USB/无线三种分支；模式切换/停止优先、复制竞争、超时、计时回绕、V2 字节与负载 |
| test_control.c | 归一化误差、PD/输出变化率、人工解锁、各类锁存故障、随机帧序列 |
| test_drive.c | 默认关闭、启用但未确认、模拟已确认配置三种分支，PWM 写入与中断安全时序 |
| test_bayes.c | 概率归一化、异常/倒序/重复数据、对称性、安装轴平移及开环 PWM |
| test_fit_model.py | 拟合输入的缺类、缺测、非法数值、时序和录制边界 |
| test_field.c | 停车调参、分段/溢出/超时、尾随指令拒绝、配置原子性、故障锁存、复位与前台/PIT1 运行租约一致性 |
| test_servo_probe.c | 舵机单独点动：电机始终零、名义首次值/步长/时限、ISR 独立撤脉冲、停止/故障/回绕、不自动确认硬件；USB/无线模式组合另在 test_debug 检查 |
| test_motor_probe.c | 单轮实验：独立电机确认门槛、另一轮/舵机为零、步长/时限/静默、ISR/故障/停止、命令历史与极性读回、回绕；USB/无线组合另测整行停止优先和图传中拒绝 |
| test_equivalence.c + reference/vision_reference.c | 历史性能优化的等价对照，当前行阈值有意改变，不再作为当前行为等价验收 |
| replay_preview.c | 显式将抓取的 94×60 预览像素 2×2 复制后回放；仅诊断，不冒充 188×120 原始帧或实车验收 |

reference 是本项目此前性能优化前的实现快照，不加入 MCU 工程。行内独立大津阈值、跟踪和叠线已有有意行为修正，**不在历史等价承诺中**；该旧对照不属于默认 run_tests。默认运行器中的示例硬件宏只用于模拟，绝不作为实际标定值。

2026-10-19 新增亮灰地板、白通道和少量远侧暗物体的曝光回归：旧极值映射阈值复现 NO_LANE/q=0，新行内阈值为 TRACK/q=100。它是人工构造的故障机制，不是复原的现场完整原始帧。预览回放保留分辨率限制，不能由回放覆盖分数推断板端实时覆盖。

断言总数包含逐像素、坐标及状态检查，不等同于独立场景数。实际记录见 [VERIFICATION](../../docs/VERIFICATION.md)。
