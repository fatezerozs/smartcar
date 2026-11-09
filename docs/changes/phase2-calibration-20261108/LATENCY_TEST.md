# Phase2 静止延迟采集

2026-11-09。状态：当前固件基线已采集；专用工程已烧录，Keil Programming Done/Verify OK。操作者RST后已通过COM14核对运行身份，取得256帧完整静止记录并完成校验。

## 当前实测基线

使用COM14和PHASE2_STOP_TRACE_20261109_1，只发送STOP、取得所有权、切文本及查询状态。板端C26=1/C27=1、OBSERVE；电机需求和遥测PWM均为0，速度环关闭。采集日志保存在[evidence/latency-static/baseline.log](evidence/latency-static/baseline.log)，统计在[latency-baseline.json](latency-baseline.json)。

共49条可解析处理耗时报告，proc_ms范围3—4ms，中位数3ms；报告fps范围50—50，周期报告帧龄范围4—20ms。这是约500ms一次的稀疏报告，不能作为逐帧P95/P99、最坏延迟或停车预算。帧龄是报告当刻距最近CSI收帧完成的时间，不能与proc_ms直接相加充当端到端延迟。

## 源码核定的时间含义

- zf_device_mt9v03x.c：CSI成功取出已完成缓冲后，在回调内记录capture_ms与递增sequence；不是曝光开始或精确曝光中心。
- board_camera.c/main.c：使用启动时开启的GPT1毫秒计时器。
- vision_frame.c：取序号与时间戳，拷贝后检查缓冲和序号稳定性；失败副本不进入处理。
- app_runtime.c：原proc_ms覆盖vision_midterm_process及已配置时的Bayes特征提取/更新；不含此前拷贝、此后线路数组复制和vision_drive_observe。
- vision_drive_observe包含控制观察及输出适配调用。OFF/OBSERVE下不会走与RUNNING相同的有效驱动分支，静态耗时不能代表闭环运行或未来MPC耗时。
- PWM驱动采用重载机制；寄存器提交不等于引脚实际生效。曝光、硬件重载、舵机及电机响应仍需单独证据。

## 专用工程与操作

版本：PHASE2_LATENCY_STATIC_20261109_1。工程由先前停车固件冻结副本建立，修改仅在独立.build目录；生产PWM驱动未改。专用PWM驱动在初始化和每次占空比更新时强制零值。这是源码约束，尚无本版本板上实测证据。

- 构建工程：`C:\shengchanli\smartCar\smartcar\.build\phase2-calibration-20261108\latency-static-20261109_1\latency_static.uvprojx`
- 专用下载工程：`C:\shengchanli\smartCar\smartcar\.build\phase2-calibration-20261108\latency-static-20261109_1\latency_static_download.uvprojx`
- AXF SHA256：`0dc07e584af183a0e04e87641909ebfdb46c69ebfda206e09cc218862c391299`

操作顺序：

1. 车辆原位置固定；驱动和舵机动力保持隔离，C26/C27均OFF；逻辑电源与COM14保留。
2. 使用上述专用下载工程下载；下载后说明版本及Verify结果，再验证板端实际身份。当前文件存在和构建成功不代表已经烧录。
3. 我通过tools/speed_planning/export_latency_trace.py --port COM14 --output <新的证据目录> --start启动静态缓存；不发arm、心跳或运动命令，不恢复此前运动配置。
4. 约5秒采满256帧后冻结；工具逐行导出，核对generation、数量、序号及FNV1a。导出约一分钟；无完整数据或校验失败时保留原始串口字节，不报告成功。
5. 缓存不会自动清除；重采前先确认已保存，再显式清空。超时30秒或离开OBSERVE时保留部分数据，不能当作完整试验。
6. 依据逐帧数据分别统计CSI完成→拷贝开始、拷贝、视觉处理、后处理及observe调用返回，并报告序号间隙和时钟分辨率。检查通过后再安排遮挡与恢复试验。

每行8个uint32字段：seq/cap/copy/process/end/observed/flags/drops。flags位0表示收帧时间有效，位1表示observe函数实际调用并记录返回时刻。drops是累计拷贝失败数；帧序号间隙与拷贝失败不能混为同一计数。FNV1a按8字段各自小端4字节连续计算，仅用于检测传输/记录差异，不是密码学认证。

采集期间抑制原500ms视觉多行报告以减少干扰，其他传输/中断仍可能运行。因此本版本只核定静态软件路径，后续还需运行构型、正常遥测负载和物理时序核对。禁止把观测到的最大值直接声明为保证的最坏延迟。

## 验证和证据层

Keil最终构建0错误、0警告，Code74264、RO14332、RW212、ZI160424字节。Host检查覆盖：256行容量、冻结后不覆盖、采集中拒绝读行/清除、OBSERVE启动门槛、严格行号解析、模式变化终止及uint32回绕超时。另以同一256行数据核对C/Python校验值一致，并验证篡改行被拒绝。首次编译的const参数赋值错误已在私有驱动定义中修正，最终构建日志已保存。

JSON/XML为structured_parsed；C源码直接核查；日志统计、哈希、构建及测试为computed。本轮无PDF来源，无仅md_extracted或新增pdf_verified结论。已烧录且Verify OK；首次烧录后无运行回复，操作者RST后已确认运行版本并取得完整逐帧数据；硬件零PWM及端到端延迟仍未独立核定。

## 第一段逐帧实测结果

固件PHASE2_LATENCY_STATIC_20261109_1；COM14；OFF/OBSERVE；generation=1，256帧；首末CSI时间跨度5097ms。逐行序号、字段、generation、冻结元数据及FNV1a校验通过。缓存保留，未清空；原始串口字节日志、trace及分析在[evidence/latency-static/session01](evidence/latency-static/session01/trace.json)。

| 软件时段 | 最小ms | P50 ms | P95 ms | P99 ms | 本段最大ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| CSI完成→拷贝开始 | 0 | 0 | 0 | 0 | 1 |
| 拷贝开始→处理开始 | 0 | 0 | 0.25 | 1 | 1 |
| 视觉处理及已配置时的Bayes处理 | 3 | 3 | 4 | 4 | 4 |
| 视觉结束→observe返回（含线路复制等后处理） | 0 | 0 | 0 | 1 | 1 |
| CSI完成→observe返回 | 3 | 3 | 4 | 4 | 4 |

255个相邻CSI间隔为19—20ms，中位数20ms；捕获序号缺口0，首末累计拷贝失败计数均0。时间分辨率1ms，表内0不表示物理耗时为零；0.25ms的P95来自排序线性插值，不表示时钟具有0.25ms分辨率。各时段百分位不能相加得到总时段百分位。

这一段仅约5.1秒、单一静止场景，普通500ms视觉多行报告在采集期间被抑制；OFF分支、零PWM驱动和未来RUNNING/MPC路径不同。上述最大值不是保证的最坏延迟，不能直接写入latency_s生产参数。采后另外留存一张场景灰度图，不是逐帧同步原图，不用于构造完整失效/恢复session。
