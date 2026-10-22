> 历史归档：本文只描述原版本，不作为当前启动、参数或验收说明。当前现状见 [项目设计与演进档案](../../PROJECT_EVOLUTION.md)。

# 当前交接：2026-10-20 无线串口与外设基线

2026-10-21更新：已完成MIDTERM_TRAIN_1静态参考轴RAM标定（0.50802/0.50535，采样带0.78/0.62/0.035），
左右平移near正/负、左右偏航heading正/负；实体舵机视觉纠偏尚未测试，MIDTERM_VISUAL_REVIEWED仍0。
源速度参数已填写40/1/0.05、符号-1/+1、MIDTERM_SPEED_CONFIRMED=1；静态现场未启动电机，未保存Flash。
详情见[MIDTERM_STAGE1](MIDTERM_STAGE1.md)。本段优先于下面历史状态中的待标定描述。

最新成果：ENCODER_SPEED_DIAG_5已完成用户直线、主动STOP、失联停车、往复倒退和左右短圆弧验收。
速度配置40/1/0.05（实际Ki=0.046875），sign3=-1/sign4=+1，闭环balance=1/1，cap0.15、rise0.1。
最新实测舵机1525右/1575左、中位1550，旧左右标注作废；视觉sign仍未验收。
同步提交MIDTERM_TRAIN_1/MIDTERM_AUTO_1代码，确认位默认0，尚未验证自主全赛道运行。
本段更新优先于下方历史交接状态，详见ENCODER_SPEED.md及MIDTERM_IMPLEMENTATION.md。

新增速度环尚未纳入此前实车验收：编码器3左/4右，实测前进符号-1/+1；DIAG_2恢复库默认计数方式后可采样。DIAG_3修正预置后首次非零速度请求误触发控制超时，新增故障子原因和时间快照，5283项主机检查与ASAN通过；待用户烧录复测，见 [ENCODER_SPEED](../../reference/ENCODER_SPEED.md)。原MOTION_DIAG_2提交仍是已验收开环基线。

维护分支main，实车版本MOTION_DIAG_2。用户已确认双串口、两轮/舵机点动、落地直线、限时及!停止、按R确认倒退和左右短圆弧正常。
获取仓库后先读 [BASELINE_20261020](BASELINE_20261020.md)，再按 [MOTION_DIAG](../../reference/MOTION_DIAG.md) 生成独立工程。
不要使用根目录旧工程或Example产物代替新工程；生成AXF和个人Keil下载器设置不入库。

- 无线UART8：D16 TX/D17 RX、D26 RTS、D27 CMD；有线UART1并行，均115200/8N1。
- 停车x接管，当前控制端h每100ms续期，任一端!可停。@行必须CRLF；500ms无有效心跳撤输出。
- M1右轮C6 PWM/C7 DIR、M2左轮C8 PWM/C9 DIR，前进电平均1；公共150‰、balance=1/0.667得到右15%/左10%。
- 舵机C30，center/min/max=1550/1500/1600，sign=+1。旧JSON中的-1不适用于本次安装。
- C12及四键控制已移除，C26/C27保留选档和切档停机；上电/配置恢复不自动发车。
- 运动版修复PWM保持期间重复装载，各路仅变值写入，停车强制写零并清缓存；不绕过PIT1安全检查。
- 运动只用开环参考PWM和固定时长，未完成完整圆周/自主循迹/停车距离/真实无线拔线/NOR掉电恢复验收。

机器可读参数见 [本次记录](../../../profiles/motion-baseline-20261020.json)，它是证据文件，不会自动恢复标定或启动。
普通测试、双串口、运动及其ASAN、ARM和Keil构建结果见 [VERIFICATION](VERIFICATION.md)。

## 以下为合并前历史交接

下文vision-debug、C12自动启动、sign=-1和“尚未实测”为旧版本记录，不作为当前使用说明。

# 队友接手：2026-10-19 现场成果 / 2026-10-20 整理上传

维护分支：`vision-debug`；本次不修改 `main`，不强制推送。最新代码标识 `MIDTERM_FIELD_4`。历史现场记录和 FIELD_1～3 说明仅作为排障依据，当前启动规则以 [C12_STARTUP](C12_STARTUP.md) 为准。

## 获取与打开

```powershell
git clone --branch vision-debug https://github.com/fatezerozs/smartcar.git
cd smartcar
```

Keil 打开 `demo/mdk/rt1064.uvprojx`，目标 `nor_sdram_zf_dtcm`。ARMCLANG 6.21，NXP.MIMXRT1064_DFP 19.0.0。所有源码引用相对工程目录，不需要原作者的绝对路径。原 Camera/Island/PID 源文件保留，但不再由当前 MDK 目标运行。

提交只包含源代码、工程、离线测试、工具和说明。`.build/` 现场串口日志、相机图片、Objects/Listings、个人 Keil 会话和下载器选项不上传；每位队友本机重新编译并选择自己的下载器。不要仅拷贝一个 main.c。

## 有效成果

- 无编码器中期链：摄像头采集 → 行内自适应阈值与通道边界 → 视觉转向闭环 → 电机开环 PWM → 单一安全输出层。没有初始化/读取故障编码器，没有旧速度 PID 覆盖停车结果。
- 视觉与安装：归一化几何、停车可配近/远行带及两点相机参考轴；行带修改清除旧标定资格。行内 Otsu 改进亮灰背景/局部暗物体吞边问题，合成夹具验证，不冒称场地泛化已验收。
- 电机：PWM 上限、缓升、视觉难度降额及两路独立衰减 @balance。PWM 不是实测车速，不能检测物理堵转/打滑/欠压。
- 启停：用户指定 C12，内部上拉、接信号 GND 开启，断开停止；恢复完整保存配置后，本地延时 2 秒尝试启动，不需电脑 h 心跳。保留远程 a/h 调试模式，完整赛道 f 模式仍只观察。
- 异常管理：PIT1 独立检查帧龄/运行租约/C12，参数修改、串口 STOP、图传或暂停先停车。故障锁存，无效启动期限不在之后补启动，短开关抖动不会重新发车。默认单次上限 5 秒，停车可配 @duration 并保存。
- 保存：@save/@load/@startup、双页 CRC32/版本/序号/读回验证，链接器预留 Flash 最后 8KiB，失败可回退上一有效记录；未知非空页拒绝擦除。保存不立即启动，不保存运行状态或故障。
- 通信：保留 USB/UART8 无线、原 RX/CSI/VSYNC 中断；逐飞 V2 的 94×60 预览头与数据不混文字；参数分段解析、停止优先及严格数值验证。
- 贝叶斯：默认模型未训练，仅显示 candidate；即使后验很尖锐也不接受语义分类，不把后验冒充正确率。中期驾驶不依赖这些未训练赛道标签。

## 验证边界——不要当作已经跑完整圈

完整 Keil 重建 0 Error / 0 Warning；主机 /O2、AddressSanitizer、ARM 严格编译与工程引用检查通过，详见 [VERIFICATION](VERIFICATION.md)。测试包含实际主循环的 USB/无线集成、C12 两秒启动/停止/无延迟重试、CRC 单比特破坏、模拟写断电及计时回绕。

最后由助手明确核对的板上版本为 FIELD_3；FIELD_4 在本次交付前只完成离线编译，**没有确认 FIELD_4 实车下载、C12 实际接线、NOR 实际写入或动态赛道通过**。这次上传也不执行烧录/运动。Git 上源代码成功上传不代表车上固件已更新。

实车已观察到的只有舵机名义中位 1550µs、窄窗口 1500～1600µs、right_sign=-1，保存在 `profiles/field-20261019.json`，不是机械硬极限或通用参数。电机前进极性、起步 PWM、当前安装下有效的相机参考轴仍待确定；非赛道画面的 NO_LANE 是合理拒识，不能拿来确认识别算法故障。

**空白存储、未完成配置时，仅烧录不会跑。** 不要为了“让它动”删掉保护或伪造标定位。应先停车完成真实参数、检查 ready、显式保存，再依 C12 流程启动。驱动板电源没有反馈，先准备驱动供电，再启动主板；建议 C12 断开上电，再合上开关计时。

## 修改入口与后续重点

| 内容 | 文件 |
| --- | --- |
| 视觉边界/阈值 | demo/code/vision_midterm.c |
| 集中默认值及硬件时限 | demo/code/vision_profile.h |
| 视觉转向及控制状态 | demo/code/vision_control.c |
| 电机开环调度与配平 | demo/code/midterm_open_loop.c |
| C12、PWM 和安全租约 | demo/code/vision_drive.c；demo/user/src/isr.c |
| 运行参数/串口命令 | demo/code/vision_field.c；vision_debug.c |
| 保存格式/板级 Flash 适配 | demo/code/vision_storage.c；vision_storage_board.c |
| Flash 预留边界 | demo/mdk/scf/MIMXRT1064xxxxx_flexspi_nor.scf |
| 贝叶斯语义观察/模型 | demo/code/track_bayes.c；docs/archive/history/BAYES_REFACTOR.md |

后续优先做真实车辆配置、C12 极性/接线确认、停车状态 NOR 保存和重启恢复、短时低 PWM 运行与转向调整，再做直道/左右弯道；未训练完整赛道模型不可直接用于自主环岛/十字控制。安装变化后重新确定参考轴，不把某帧像素位置硬编码为通用道路。

## 复现测试与工具

```powershell
cmd /c tests\vision\run_tests.cmd
cmd /c "tests\vision\run_tests.cmd asan"
.\tools\verify_project.ps1
.\tools\check_arm.ps1
.\tools\build_keil.ps1
```

主机测试需 MSVC C++ Build Tools（ASAN 需相应组件）。ARM/全工程构建需本机 Keil，可传工具路径。命令均不驱动车辆。

现场工具 `field_serial.ps1` 默认只读，端口用实际 COM，不硬编码 COM9。图传抓取工具先停车；`apply_field_profile.ps1` 只恢复已观测舵机和行带、强制电机零配置，不假装自动标定。`flash_keil.ps1` 要求 StoppedAndReady 和 C12Open；保存需 SaveForAutostart，提醒下次上电可能启动。重新下载若整片擦除会清参数，应检查 STORE 读回。

阅读顺序：[C12 启动](C12_STARTUP.md) → [工程说明](PROJECT_GUIDE.md) → [验证记录](VERIFICATION.md)；MIDTERM_FIELD/OPEN_LOOP_FIELD_3/MOTOR_BENCH/SERVO_CALIBRATION 保留现场历史，不覆盖最新启动约定。
