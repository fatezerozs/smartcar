# 联合静止验证重试 — 2026-11-08

结论：Camera/SD/控制调度的静止短窗与PC完整回读已取得，OFF→ON→OFF相位完整；有效PI并行未覆盖，联合总验收仍不通过。

## 本轮修正

原联合适配在100帧上限之后继续发布视觉图像，同时仍累计视觉计数，出现observed_capture=100、vision_processed_frames=101。生成器现限定计数序号为base_source < sequence <= last_source，连续视觉处理不停止，PC完整性断言未放宽。实际生成的Vision任务边界回归在旧实现失败、修正后通过；另17项Recorder回归、联合控制/统计Host与Keil构建通过。

修改源文件：tools/prepare_recorder_joint.py；tools/test_recorder_joint_static.ps1新增BuildDirectory参数；tests/recorder/test_joint_capture_window.py新增定向回归。

## 构建与身份

工程：C:/shengchanli/smartCar/smartcar/.build/recorder-joint-20261106/run-20261108T122408-36056-final/candidate.uvprojx。
输入摘要：7900b6626a301d0471d4d513551c4835b3f98111bce09e7dab63e00a1bc83c14。468项源码与冻结清单核对一致；两处PWM入口仍无条件强制零。首次换烧实际回复seed摘要，未发送START；换烧final后通过身份门。

本地AXF在人工换烧期间再次构建。当前AXF导出的123712字节load image与原final BIN仅3字节不同，全部位于编译时间字符串（20:24:33→20:27:06），其余字节一致；Session的自报时间与当前镜像一致。本地重建来源一致性已有证据，未独立读取板载Flash，不宣称板端字节已逐一核实。实际本地镜像快照、差异和哈希已保存。

## 实板结果

- 成功Session：2:/R0000005，100帧，序号1682—1781，约50.025FPS。written=synced=100、storage=4、error=0。
- OFF基线、写入、收尾、OFF尾段均有相位报告。10ms采样，控制服务最大间隔1ms，10ms服务预算超限0；实际PWM非零0，IO错误0，记录队列/池丢帧0。队列峰值41/64。统计是该仪表化构型观测，非WCET。
- PC逐块CRC、全部帧解析/像素往返、连续序号与时间戳校验通过；Session状态complete。结束计数observed_capture=accepted=recorded_frames=vision_processed_frames=100。
- 写入窗口pi_consumed=0、PI故障位0；不能把空闲PI调用算闭环。OFF尾段正常、LAST_STOP=0；不能据此证明正常RUNNING到OFF切断。
- 对全部100张原始图像运行原生C31视觉Host adapter：valid=0/100、status=VISION_NO_LANE为100/100。原图可见赛道与室内背景；“面对赛道”尚未形成算法有效输入。回放冷启动且不含板端Bayes历史，不等同于逐帧板端状态；但与未准入/零PI的实测一致，仍需板端准入诊断或合格直线场景确认具体拒绝门。

## 后续与结束状态

下一轮先让相机视野中的直线段满足近场/边界要求，必要时添加只读的准入拒绝原因遥测，再取得写卡窗口有效PI消费，保持全部零输出约束。不能关闭视觉保护或修改验收断言凑PASS。单Session限制要求人工复位/重新上电，不自动追加硬件回合。

结束时C26 OFF，COM14已恢复115200并关闭；未自动烧录/复位或恢复动力。旧R0000004与失败日志原样保留。

证据根：C:/shengchanli/smartCar/joint_static_20261108T122742419Z/，含serial.log、R0000005、pc、vision-replay、joint-review.json、image-provenance-review.json和tested-image。acquisition.json记录原客户端严格失败，后续报告不覆盖它。

读取层：原生代码/JSON/日志为structured_parsed；CRC、像素往返、回放、帧率和镜像差异为computed；PNG原图直接抽样检查。无md_extracted或pdf_verified。没有真实动力、运动或长期稳定性验收。
