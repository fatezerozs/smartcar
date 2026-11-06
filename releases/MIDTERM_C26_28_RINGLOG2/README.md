# RINGLOG2：绕环阶段连续性与白色连接区

2026-11-07。基于用户确认本次入环有所改善的RINGLOG1继续修改，不更换基线。RINGLOG1原固件/完整源码存档不覆盖。

## 本轮真实证据

已观看用户提供的bd76445f954717deb6b15196ac48cb37、e08de6e380ee601b560c091834d77200、ef5deecaa6417ba5dfb2529c60c1705b三段视频，时长38.50、17.70、15.17秒。用户确认主板仍供电、C26 OFF，随后接好下载器。

实际下载器为SEEKFREE ZF-WLFS-CMSIS-DAP（80982440-206EF1AD3E8C），不是原导出工具假设的J-Link。首次J-Link未连接成功，没有执行复位或烧录。改用pyOCD 0.46.1、generic cortex_m、connect_mode=attach、auto_unlock=false、resume_on_disconnect=false读取描述符，确认recording=0后短暂停核读出RAM，按原状态恢复运行；没有reset/download/flash。

导出1422558字节、4310条记录、4135控制样本、166张2Hz预览，前后描述符一致、双CRC全部通过，dropped=0。SD状态报告sd_state=2、error=0、flushed=used，文件2:/R0000000.BIN。此次读取为停车RAM副本，板端报告已写SD，不把它冒充SD文件的独立PC回读。原数据保留recordings/dap-export-20261107-005341-808450/，本版本另存runlog1-evidence.zip。

共4次C26窗口：第1段约0.825秒；后三段分别43.391、20.750、17.410秒。视频与这些记录没有严格时码同步，按后三段分析，全部保留。实测ring状态只有0/1，没有FOLLOW/EXIT。run3在58312ms回退并清除ENTER，58452ms视觉停车；run4在89059ms清除ENTER，89199ms视觉停车，均相隔140ms。run2在42579ms仍处ENTER时视觉停车。后两段还在68568/99074ms再次视觉停车。没有把这些窗口里的停车归为编码器故障。

## 修改

1. 已确认岛的当前内侧弧线足够长、近远端同向弯曲且有当前白色车身通道时，允许在外侧近边界被裁切/开口后确认FOLLOW。使用路宽归一化弯曲量，不要求以前闭合岛帽必须到某个固定图像行。实际外边界仍完整可测的相切直路不使用此分支。
2. FOLLOW遇到白色展开区、近端边界暂失时，先尝试把当前近端白区连接到8行当前真实远端边界。退出资格未成立前，优先锁定岛侧分支；每行白路和原连接曲率/宽度检查保留。这里输出的是当前绕环连接路径，不自动切EXIT。
3. 暂时退回当前可行普通白路不再在3帧后清除已经建立的FOLLOW。持续反向曲率否定、OFF/reset及原出口完成流程保留。
4. EXIT资格仍要求实际外侧弧线证据和当前出口路径的多帧确认。新的裁切内弧本身不置exit_armed，入口开口不能立即当出口。

应用已有文件仅vision_reference_route.h及固件名vision_profile.h变化。入环控制vision_control.c、完整入口生成器vision_ring_entry.h、发车、C26、执行器、轮速PI/速度参数、主循环和SD记录源码均与RINGLOG1逐字节一致。没有新增定时转向、强推PWM或改变正常发车条件。

## 验证

最终裁切回归32项：冻结RINGLOG1失败8项，RINGLOG2全部通过；包含左右镜像、两种岛帽位置、同一开口不得直接EXIT及原相切直路反例。完整ASAN通过：C26原有启停300852项、普通转向382878项、环岛258项、出口207项、阶段32项、40组投影979513项。带SD日志的真实主循环另行ASAN通过；日志核心/FatFs故障检查、Python解析4组、协议79项、基础工程379引用和生成工程378引用/内存审计通过。Keil0错误0警告。

用本轮94×60预览放大成188×120、每张重复25次作固定图像几何对照。没有模拟车辆位姿随控制变化，不能代替动态仿真或实车通过，像素和帧间过程也不等同原始50Hz图像。相同回放输入下，原版三个窗口只有ENTER；候选三段均可FOLLOW，后两段在66429/97435ms预览处选择EXIT，两个原停顿附近可保持当前有效连接路径。run2的预览在第一次停车后没有更多运动输入，未宣称该段出环。回放源码/结果在证据包中，以诊断形式保存，不作为生产初始化流程。

本版只构建，没有烧录、发车或实车验收。下一次实车继续使用现有相机安装和C26；重点看连接区不再出现短暂停车、沿环保持和正确出口。无需再做重复静态采集。

## 烧录和导出

Keil打开：C:\Users\dick\Desktop\智能车\c26-28-debug-20261104\smartcar-8453672\demo\mdk\midterm_switch.uvprojx
目标nor_sdram_zf_dtcm；固件标识MIDTERM_C26_28_RINGLOG2。C26 ON发车、OFF停止。

跑完保持主板供电、C26 OFF，不复位/再次烧录，接好当前CMSIS-DAP后由助手导出。匹配工具为tools/export_runlog_dap.py：

```powershell
C:\python\python.exe tools/export_runlog_dap.py --parked --probe 80982440-206EF1AD3E8C --version MIDTERM_C26_28_RINGLOG2
```

项目隔离依赖记录在tools/requirements_debug.txt；本机已安装在.build/debug-deps。独立环境可用python -m pip install --target .build/debug-deps -r tools/requirements_debug.txt安装。关于attach等选项参见https://pyocd.io/docs/options.html 。保留J-Link工具供真正J-Link使用，不再把本机CMSIS-DAP当J-Link。

COM15及COM17的持续只收记录器已经自动接续，30分钟只轮换文件、不关闭健康端口，无固定到期；USB断开会重试连接。原始数据按启动/复位分开分析，未发送的数据无法追补。本轮没有上传GitHub。

本存档含完整源码工程、AXF、MAP、逐文件源码哈希、修改差异、构建与测试记录、原实车日志及回放证据。source-code.zip解压后保留目录关系，打开demo/mdk/midterm_switch.uvprojx，可用已安装Keil/设备包重建。

AXF SHA256：072CEA85BC571A0CAC1D7CE0D12C9A81AC812455E1B4AB1FA88C41F43CB6CC15
