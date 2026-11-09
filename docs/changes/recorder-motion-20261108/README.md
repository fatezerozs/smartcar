# 当前入口：main原生无线迁移

当前候选和模块验收见[MAIN_WIRELESS_MODULE.md](MAIN_WIRELESS_MODULE.md)。旧RMR1方案停止推进，以下为历史记录。

---

# 当前候选：无线串口版本

2026-11-08：用户指定无线串口收发。当前端口枚举为COM3/4/6/7/10，COM14不在列表；此前USB身份失联不能据此直接认定程序异常。原运动候选命令只消费APP_LINK_USB且printf走USB，需要换烧通信适配，不能只改PC端口。

**当前无线工程**：C:/shengchanli/smartCar/smartcar/.build/recorder-joint-20261106/motion-radio-20261108T132744938738Z/candidate.uvprojx。
AXF：同目录Objects/recorder_motion_c26_3s.axf。
source：3c2e93b75fbdab2067fde598ea571f4fddec6e76eb97cf73a6cdeb6b275ccd69。

命令从APP_LINK_RADIO解析，报告经有界app_link队列/原生RTS门发送，后台由Monitor轮询；SD只读导出的头/二进制/尾按同一队列发送。RF模块与MCU均固定115200，不允许921600切换。运动许可、计数窗口和保护不变。实际RADIO格式化/原始二进制/队列反压/调度前等待/3秒发送超时Host测试通过，Keil0错误0警告，构建输入哈希复核通过。

测试入口：tools/test_recorder_motion.ps1 -Port COM10 -BuildDirectory <当前无线目录>。只读导出客户端需保持115200，不复用原USB快速BAUD握手。等待用户换烧与Run，C26 OFF、动力隔离；仅通信准备不计运动完成。

证据层：代码/JSON/XML为structured_parsed；构建、Host与哈希为computed；无线板端收发尚未验证。下面保留旧候选和故障历史。

---

# 当前候选更新：启动顺序修正

2026-11-08：两次仅STATUS查询均无回复，未发送START/ARM，动力保持断开。代码检查发现运动PWM适配在main的timer/GPIO初始化之前调用timer_get/gpio_get_level；这是一处启动顺序缺陷，可能导致UART初始化前停滞，尚未通过板端调试确认唯一根因。

已将未arm/已expired的执行器输出改为先返回零，不访问外设；新增实际motion_guard C代码的Host测试，验证pre-arm零外设访问、限幅、三路独立切断和到期锁止。Keil重建0错误0警告；实际输入摘要已核对。

**当前待换烧工程**：C:/shengchanli/smartCar/smartcar/.build/recorder-joint-20261106/motion-20261108T124510099831Z/candidate.uvprojx。
AXF：同目录Objects/recorder_motion_c26_3s.axf。
摘要：74a82b33dcf2cf1707eebf0966cd07c317df20e626446ee40752a45f58cf5262。

旧123934候选不再用于后续试验。下面保留首轮准备记录；实际运动仍未执行，等待修正版人工换烧及身份回复。

---

# Recorder 首轮落地短时运动试验准备

2026-11-08：用户明确授权运动，选择落地低速短时、最长3秒；现场5米无障碍直线，有人工断电急停。用户要求暂按PI有效推进，记录为USER_WAIVED_NOT_MEASURED_PASS；保留静止pi_consumed=0、离线VISION_NO_LANE与联合整体未通过证据，不更改为实测PASS。

## 本轮候选

独立工程：C:/shengchanli/smartCar/smartcar/.build/recorder-joint-20261106/motion-20261108T123934314173Z/candidate.uvprojx。
AXF：同目录Objects/recorder_motion_c26_3s.axf。
固件ID：RECORDER_MOTION_C26_3S_1；source摘要：8d3e0b53073aeab5bd14be6b44ffc4ea9891ab6175242a3e52ab8cb1e1004bfd。

- 基于联合候选复用C26原生控制/Encoder/PI、Camera/RTOS/SD，控制及视觉保护不绕过。
- 硬件独立PWM适配：两个入口（init/set）均经过motion_duty；驱动上限2000/10000（20%），请求轮速上限20 counts/10ms；尚无负载周长，不能声称为已标定m/s。
- 初始全零，MOTION START仅启动相机/调度，MOTION ARM只能在OFF状态接受；5秒OFF基线后等待人工ON，ON稳定后启用一次2900ms许可。
- 最终PWM写入门每次检查原始C26和2900ms期限；10ms PIT独立tick到期或OFF后清零三路PWM、请求停车和记录收尾，锁止直到MCU复位。2900ms为输出指令窗口，非车辆物理停车时间承诺；切断时刻还须板端核验。
- 保留视觉准入/Encoder/帧超时等原生保护，不能用本次授权把不合格视觉强制启动。
- 记录硬上限200帧，另有4000ms软收尾；2900ms许可到期会提前请求记录结束，允许Writer在输出切断后排空、同步、关闭。
- 仅创建新Session，沿用2:、1-bit/3.3V SD与只读导出，不格式化或覆盖旧文件。

## 软件检查

Keil 0错误0警告；记录实际源码/头文件/工程/链接布局输入摘要、AXF/BIN哈希，MAP含motion_arm/motion_duty/motion_tick并检查内存未越界。实际限幅策略C测试覆盖未授权零输出、20%封顶、期限边界、OFF、一次性锁止及32位时钟回绕。运动串口客户端编译及解析自检通过；既有联合控制/统计Host与17项Recorder回归已在本轮静止修复后通过。以上不等于新带动力固件实板时序或机械安全验证。

客户端入口：tools/test_recorder_motion.ps1 -BuildDirectory <上述目录>，只在操作者换烧、Run和现场就绪后执行。客户端核对固件身份/摘要/OFF再发送START和ARM；读取分相位报告、MOTION LIMIT与END。允许实际非零输出，但要求MOTION LIMIT报告峰值<=2000、截止耗时<=3000ms；仍单独报告PI消费为0及物理未运动，不伪造通过。

## 当前状态

候选构建完成，等待人工换烧并保持C26 OFF/动力断开。实际运动尚未执行。准备过程中没有打开串口、烧录、复位或恢复动力。

证据层：原生C/XML/JSON为structured_parsed；构建、MAP、哈希和C策略测试为computed。无md_extracted/pdf_verified；实际速度、距离、转角、停车距离和运动完整性仍未知。
