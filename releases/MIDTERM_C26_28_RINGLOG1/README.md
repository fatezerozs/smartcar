# RINGLOG1：入环路径优先与停车后SD记录

2026-11-07。沿用用户指定的 midterm_switch 原工程。上一交付 ARC1 的源码与固件保留在独立 releases/MIDTERM_C26_28_ARC1，不覆盖。

## 这次改变

当前已确认入环且存在这一帧白路引导时，由入环目标决定分支。原版入口引导和连通入口规划均保留：连通路径同时参与近端与前瞻；短入环路径即使位于普通远端采样带以下，也会被实际转向读取。原版白路引导同样参与前瞻，直道的反向居中/微分项只能修整入环转向，不能取消该白路分支。FOLLOW/EXIT不叠加入环偏移。没有固定先左后右、按时间盲转或关闭断流/编码器保护。

修改前的实际控制链测试出现反向转向；修复后的左右方向×原版/连通两种入口均通过，转向分别约−0.468/+0.510、−0.805/+0.815。原发车模块、C26模式、舵机执行器、轮速PI、速度参数和入口生成器与ARC1逐字节相同。应用原有文件只修改vision_control.c、app_runtime.c和固件名；新增日志文件及隔离的SD驱动。

## 日志怎样记录

C26 ON开始记录：每个已处理相机帧记录控制/视觉/环岛/左右轮目标与实测值，并在控制状态/故障变化时追加样本；包含同一帧号的94×60灰度预览、左右边界/中心/入口引导/行有效标志，图像按2Hz采样。状态事件保留ON/OFF及run编号。图像是预览采样，不是50Hz完整原始摄像头流；速度保留counts/10ms，不冒充米制标定。

运行时只向8MiB SDRAM追加，预留最后1MiB给控制数据。满后明确累计dropped，记录故障不让小车停车，也不声称可无限记录。C26 OFF、模式稳定且输出已停止后，先清理缓存供调试器读取，再以32KiB分块写SD，完成后sync/close。main分支独立B2曾测得sync约157ms，故本工程不在行驶主循环中同步写卡。未引入Recorder的RTOS或其静止锁定/测试流程。

SD使用main参考的USDHC1、1-bit、3.3V、最高25MHz与独立DMA中转区，只使用CMD/CLK/DATA0；保留D16/D17无线与C30舵机，不调用会占用这些引脚的通用四位初始化。SDK原文件没有修改；使用私有策略包装。电源与卡检测使用本工程GPIO接口，补齐独立诊断程序原本在main中完成的DWT计时初始化。

文件名为SD的2:/R0000000.BIN起递增，以FA_CREATE_NEW找新文件，不覆盖原有文件；一次上电可保存多次C26启停，记录带run编号。停车再次运行时继续同一内存/SD会话；重新上电创建新文件。没有格式化、删除旧文件或自动烧录操作。SD失败保留内存日志与错误码，sd=3，不以日志故障新增驱动锁止。sd=2且saved=used表示已同步关闭。

## 跑完后用户做什么

1. C26拨OFF，保持主板供电，不按RST、不再次烧录。
2. 接上J-Link下载器；不要先进入Keil Debug，因为该操作可能配置为下载或复位。
3. 告诉助手已接好，由助手导出。默认等SD写完；若卡异常，可独立恢复已发布的停车RAM，不把RAM导出成功当成SD落盘成功。

导出工具 tools/export_runlog.py 通过J-Link读DTCM描述符及对应SDRAM，检查版本、地址/长度、记录数、前后状态和每条记录的双CRC。脚本只halt/savebin/go，不含reset、download、erase或电机命令。旧版J-Link使用ASCII临时路径处理中文工程目录。输出到新的 recordings/debug-export-时间/，有原始二进制、metrics.csv、带帧号PNG及summary.json。用户当前电脑的J-Link DLL已离线确认支持MIMXRT1064xxx6A；尚未用新固件执行板端导出。

手动导出（通常由助手执行）：

```powershell
C:\python\python.exe "C:\Users\dick\Desktop\智能车\c26-28-debug-20261104\smartcar-8453672\tools\export_runlog.py" --parked
```

也可以将SD上的Rxxxxxxx.BIN复制到电脑，用tools/read_runlog.py解析。已关闭文件保留在SD；上面的默认调试器导出读的是本次上电的停车内存副本，断电后不能再用该RAM入口恢复旧会话。

## 验证及边界

Keil 0错误0警告。完整ASAN通过：C26原有启停、普通转向382878项、环岛258项、出口207项、阶段22项、地图投影979513项；带日志开启的真实主循环另行ASAN通过。日志核心检查CRC、启停、重复会话、行驶不做IO、容量溢出；FatFs替身检查新建不覆盖/追加/分块和7类IO失败；Python解析/损坏拒绝/描述符/无复位命令4组通过。协议77项；原基础工程379引用及生成switch工程378项路径/SD隔离/内存审计通过。

MAP确认8MiB日志位于缓存SDRAM，104字节调试描述符位于DTCM；卡/host/DMA描述符/bounce位于64字节对齐的非缓存SDRAM，未占用相机或控制栈。预览临时区为静态存储。

本版仅构建，未烧录、未发车。新固件的运动+日志/SD、实际时延和调试器导出尚未实板验收；软件测试不代表环岛已实车完成。相机本轮保持当前位置。无线接收继续尽可能被动记录；23:32段到00:02:50自动到期，00:04:32续开，中间约102秒没有记录，续开后本轮检查尚无新字节，不能冒称已采到本版实测。

main参考固定提交：00bb7a47ee5b15fe845137895316d54ef24f6a1d

- https://github.com/fatezerozs/smartcar/blob/00bb7a47ee5b15fe845137895316d54ef24f6a1d/demo/sd_performance/board.c
- https://github.com/fatezerozs/smartcar/blob/00bb7a47ee5b15fe845137895316d54ef24f6a1d/demo/sd_performance/host_policy.c
- https://github.com/fatezerozs/smartcar/blob/00bb7a47ee5b15fe845137895316d54ef24f6a1d/demo/recorder/recorder_sd_pins.c
- https://github.com/fatezerozs/smartcar/blob/00bb7a47ee5b15fe845137895316d54ef24f6a1d/docs/changes/vehicle-parameters-20261106/README.md
- J-Link命令参考：https://kb.segger.com/J-Link_Commander

烧录工程：C:\Users\dick\Desktop\智能车\c26-28-debug-20261104\smartcar-8453672\demo\mdk\midterm_switch.uvprojx
目标：nor_sdram_zf_dtcm。source-code.zip为本版完整源码工程快照，含SDK、项目、工具及测试源码，解压后保留目录关系即可打开demo/mdk/midterm_switch.uvprojx。设备包和Keil仍使用本机安装。

AXF SHA256：078C2E4CCD8E1EDD468C4249B5E1F9F14A60B946C79A4E5CF7CF52DC3B6681F4
