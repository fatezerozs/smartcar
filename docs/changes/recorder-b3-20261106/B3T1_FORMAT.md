# B3T1：静止RAM事件契约 v1

本版是独立C26静止诊断trace，不替换或暗改SCRAW-1图像协议，也不是已完成的B3运动Session。正式运动持久化sidecar还需boot/session标识、权限/提交因果与图像文件配对，必须显式版本化；旧静止reader拒绝未知格式。

## 帧与时间

ASCII数据头B3TRACE DATA，含format=B3T1、run/count/bytes/crc32/lost/reason/hz、PI/PRIMASK/emit最大周期、m1/m2/servo引脚枚举、static=1、synthetic=0/1、source输入SHA。紧随count×64字节little-endian事件，末尾CRLF B3TRACE END CRLF。CRC32沿用Recorder C实现，PC用独立zlib核对；不接受短读/重复字段/未知版本。

每事件64B：ordinal/kind/ms/cycles各uint32（16B），v[12]各int32（48B）。ordinal从1递增，源偏移=(ordinal-1)×64；ms为同源GPT1毫秒发布时刻，cycles为DWT快照。PC对32bit毫秒合法回绕扩展，拒绝小幅倒退。捕获时刻另存VISION.v1，不能用处理/提交时刻替代；周期差值仅对小于DWT回绕周期的局部区间有效。run仅本次上电局部序号，不能作全局SessionID，主机唯一目录另行区分。

| kind | v字段含义（未列字段保留0） |
| --- | --- |
| 1 RAW_COUNTER | v0 Encoder3左绝对16-bit计数，v1 Encoder4右绝对16-bit计数；未清零/未推导倍频 |
| 2 SAMPLE | v0 native sample序号，v1 sample_ms，v2/3软件归一化raw3/raw4（counts/10ms），v4/5 total3/total4，v6/7 target_right/left，v8/9 PI PWM right/left，v10 speed faults，v11同次RAW事件ordinal引用 |
| 3 PI_IDLE / 9 PI_WORK | v0 sample_seq，v1 sample_ms，v2/3左右目标（右/左，ticks/10ms），v4/5 measured_right/left，v6/7 PI输出permille，v8 faults，v9函数区间cycles，v10原mask区间cycles，v11原临界区冻结的last_consumed样本序号 |
| 4 DRIVER_WRITE | v0 pin，v1请求duty，v2实际传给SDK的duty（强制0），v3..6 VAL2/3/4/5，v7 MCTRL，v8通道0右/1左/2舵机，v9 PWM_DUTY_MAX=10000，v10请求已知 |
| 5 MODE | v0/1 C26/C27原始电平，v2 raw_mode，v3 selected_mode，v4 settled，v5 revision；C26高为OFF、低为ON；C27不拥有route启动权 |
| 6 VISION | v0 capture_seq，v1 capture_ms，v2/3 observation/far valid，v4..8 IEEE754 float原位bits（lateral/heading/steering/near_center/far_center），v9 state，v10 faults，v11 recovering与search_source位图 |
| 7 STATE | v0 control state，v1 control faults，v2/3观测/远场valid，v4 recovering；这是观测到的转换，不是外部物理事件时刻 |
| 8 PWM_SNAPSHOT | 与kind4同布局，在采集起点读取当前VAL/MCTRL；v1为缓存的最近请求，不伪称发生了新的提交 |

actual duty采用驱动native 0..10000刻度，PI PWM为permille 0..1000，二者不能混用。舵机命令若需us换算使用native duty/(PWM_DUTY_MAX×50Hz)；本候选actual=0无舵机脉冲，不输出实测角度。VAL/MCTRL是寄存器/待装载提交证据，不是示波器波形。

## 完整与缺口

3072条硬容量，5秒软采集窗口；满时reason=2、lost至少1并冻结，不覆盖旧条目，也不假报之后所有物理事件均被精确计数。reason=1为正常窗口；reason=3手动freeze；reason=4等待C26超时。未导出非空trace禁止新capture，空超时可显式清除。C26触发只启动RAM记录，不修改控制权限或物理输出。

Host synthetic=1不能算实板；CRC/源指纹/结构完整也不能单独放行控制、SD并行或运动。原始记录及显式valid/age/ref保留；没有Encoder比例时不输出m/s，没有物理测量时不把目标、PI值或请求PWM称实际车速/输出。

## PI绑定与发布顺序

kind3为idle/拒绝路径快照，kind9为进入工作分支的快照，布局相同；v0是锁内观察到的native样本，v11是锁内last_consumed序号。工作分支不保证本次生成新PI步，需比较consumed递增和请求/输出；不能按调用次数伪造更新次数。快照在原恢复PRIMASK之前复制，区外发布，避免PIT已更新全局speed却配到旧PI结果。事件发布顺序可能晚于更新的PIT事件，关联必须用样本序号/时间，不能按ordinal假定计算因果。固定快照的开销计入被测mask；这是仪表化结果，不是无埋点周期级WCET。pi_gap_max_ms采用原服务输入now时钟，局部DWT区间含原代码及中断抢占。
