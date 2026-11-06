# B3 静止前置测试：本轮收尾报告

日期：2026-11-06（北京时间）。用户最后要求“这是最后一组授权，推进速度加快一点”；本轮停止追加人工动作和硬件操作请求。**已获得静止OFF基线、首次ON/OFF和同次上电重新授权的完整遥测；B3实际运动验收未完成，不放行恢复动力。**

## 本轮实测

| 项目 | OFF基线 | 首次ON/OFF | 同次上电再次ON/OFF |
| --- | ---: | ---: | ---: |
| 完整事件数 | 1759 | 1935 | 1939 |
| Encoder样本 | 500 | 500 | 500 |
| 采样间隔min/median/max | 10/10/10ms | 10/10/10ms | 10/10/10ms |
| PI工作路径记录 | 0 | 81 | 83 |
| 不同的正consumed样本序号 | 0 | 80 | 82 |
| PI服务输入时钟最大gap | 6ms | 7ms | 7ms |
| PI函数观测最大区间 | 9.258us | 24.902us | 18.003us |
| 仪表化PI mask最大区间 | 0.232us | 18.712us | 17.927us |
| 实际传给SDK的执行器duty | 全0 | 全0 | 全0 |
| 所读PWM比较寄存器脉宽差 | 全0 | 全0 | 全0 |
| 结束状态 | DISARMED/fault0 | DISARMED/fault0 | DISARMED/fault0 |
| 数据CRC/格式/引用 | complete | complete | complete |

合计5633条事件、1500次Encoder采样。PI/PRIMASK为该静止仪表化变体的观测，不是生产WCET。记录包含251/249/248条视觉观测，但不含原始图像文件或SD Writer，因此不能代替Camera+SD+控制并行测试。

固件身份RECORDER_B3_STATIC_C26_1、outputs=forced_zero及输入指纹均与候选一致。两次ON均观测到原C26核心进入RUNNING和实际consumed递增，未由代理复位；证明本次静止实验中原生OFF/ON重新授权路径确实执行，不是Recorder训练入口的一boot一租约流程。所有三通道观测的传给SDK的duty=0，VAL比较差=0；没有测量外部PWM引脚波形、机械角或停车距离。

## 保护事件与未通过项

首次ON在本次capture范围内的LAST_STOP为seq1、faults=256、speed_detail=10（no_count_right）。第二次成功重新授权后，本次capture范围内LAST_STOP为seq3、faults=256、speed_detail=9（reverse_left）。这是用LAST_STOP原生时间落入本次记录区间来归属的；不能把日志中沿用的旧保留故障误算为本次故障。

由于电机、舵机供电隔离且软件强制0，非零目标与无/少量反馈不构成正常速度跟踪试验。首次无计数保护与此条件相符；第二次左轮反向保护说明仍需核对该时刻计数来源和实际转轮方向，不认定Encoder方向已通过。两个故障之后均在OFF稳定时回到DISARMED/fault0。

两组原始采样记录显示C26 ON持续约4.720/4.160秒；以实际时间戳为准，不用口述“约1秒”覆盖日志。保护故障发生在随后的OFF之前，因此**本轮没有建立“从正常RUNNING在故障前拨OFF”的实板切断时延，更没有真实带载停车证据**。已有Host C26断言通过不能补这个硬件缺口。

逻辑请求PWM峰值首次M1/M2约5110/4540（PWM_DUTY_MAX=10000，即51.1%/45.4%），静止适配后实际仍0。未来运动版本必须独立冻结真实PWM/速度上限，不能把现有目标比例或静止固件的请求数值当成已安全验证的行驶限幅。

## 本轮问题如何处理

1. 第一次客户端未能识别嵌在普通日志行中的B3STATIC回复；当时C26也读为ON。未发送采集命令。仅修正主机前缀提取，未换烧、未改固件；修正后实际三组完整数据验证通过。
2. 一次30秒触发等待结束时count=0、reason=4，未计测试。主机只对空、零lost的等待最多续期三次；不清除或覆盖非空未导出的trace。之后获得第二组成功ON/OFF。空等待日志保留，不伪造成功。
3. 所有完整trace均原始保存并校验CRC、索引、源指纹、样本引用及时间单调性。没有将pi_work或pc_verified自动提升为B3运动PASS。

## 证据位置

- OFF：C:/shengchanli/smartCar/b3_static_trace_20261105T215219289Z/
- ON/OFF 1：C:/shengchanli/smartCar/b3_static_trace_20261105T215450693Z/
- 再次授权：C:/shengchanli/smartCar/b3_static_trace_20261105T220024618Z/
- 初始未采集：C:/shengchanli/smartCar/b3_static_trace_20261105T215018401Z/
- 空等待：C:/shengchanli/smartCar/b3_static_trace_20261105T215747501Z/
- 联合派生数据：HARDWARE_STATIC_REVIEW.json；包含各组STATE/MODE、当前窗口LAST_STOP、请求峰值、原始计数范围及SHA256。

每个成功目录有serial.log、identity.txt、header.txt、trace.b3t、events.csv、static-review.json、acquisition.json。原始记录不可被后续派生报告覆盖。编译候选、原构建日志/MAP和425项输入清单保持不变，主机修复另记哈希。

## B3当前阶段门

| 要求 | 结果 |
| --- | --- |
| B2静止Camera+SD | 前轮通过结论保持 |
| C26 OFF下原生采样和零输出 | 已观察到，采样10ms稳定 |
| 逻辑ON、PI工作分支、同次上电重新授权 | 已观察到，原始数据完整 |
| Encoder已知单轮方向/轮端CPR | 未完成；第二组存在反向保护，不能放行 |
| OFF从正常运行切断的硬件时延 | 本轮未覆盖；不能用故障后OFF清除替代 |
| 实际负载速度/舵机角/安全限幅 | 未完成；几何截图仅标称/估算，历史脉宽只是卸载命令端点 |
| Camera+SD+控制匹配静止回归 | 未运行；当前固件没有SD运行时 |
| 持久化原图+完整控制遥测的真实运动Session | 未取得 |
| B3完成 / production-ready | **否 / 否** |

本轮不追加授权、转轮、动力恢复、烧录或运动请求。未来若恢复B3工作，以上缺口集中处理：先方向/CPR和物理尺度，再匹配SD并行静止验证，之后才是明确限速、距离、时长和急停条件的受控运动。不能直接解除当前零输出约束到赛道运行。

## 结束状态及保护

最后一次成功导出要求且读到C26 OFF/OBSERVE、非RUNNING；记录结束DISARMED/fault0。三个完整采集后COM14均在115200关闭，未打开COM10。用户确认底盘固定、电机/舵机供电隔离；代理没有恢复动力、烧录、复位、格式化、改分区、删/覆盖SD文件。

本轮新增文件与主机解析修复不改生产控制/参数/原工程。数据可信层：板端事件/CSV/JSON为structured_parsed；CRC、样本关联、周期换算、故障窗口匹配和统计为computed。没有本轮真实运动或新的物理尺度实测，不用Host仿真或标称尺寸补为PASS。
