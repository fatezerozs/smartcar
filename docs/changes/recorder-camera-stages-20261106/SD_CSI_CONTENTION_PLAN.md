# SD / CSI 资源竞争的分级对照

本轮只作源码/MAP与Host验证，实际争用指标全部UNKNOWN。

对照顺序：已存SD-only历史短测（CPU600，24.75/49.5MHz）→待批准Camera-only固定缓冲采集→待批准当前Camera+SD短窗候选→审查后延长静态采集→另行批准控制并行静止测试。B1长期模拟压测按用户跳过，因此没有B1 matched-load长期对照，必须保留此缺口。

各次冻结相同CPU、SD时钟、相机配置、像素/stride、有效帧数窗口，记录config identity和SD卡/供电/日志。比较CSI完成间隔、双标志/FIFO overrun、pool/queue溢出、Capture复制最大耗时、Writer实际调用/同步分布、RTOS堆栈峰值及数据重建结果。候选现有状态可输出queue/copy/歧义等，未实现的更细CPU总线计数不得写成已有测量。
新候选没有控制任务，不能用其结果证明控制deadline。主线无Recorder基线和带Recorder控制并行必须使用同一最终commit/PI监测契约，先不改变控制权限。伪造同宏、复用同函数或静态指令数均不能代替实际中断屏蔽与目标WCET。
任何drop/CRC/计数/END/清单异常归incomplete；如果阈值达不到便停止并保留文件，不无限重试，不格式化，不真正热插拔/突然掉电。

## 2026-11-06 实测更新

真实100帧Camera+SD短窗已完成且PC全量CRC/PNG校验通过，队列高水位32/64、显式丢帧/池耗尽/IRQ歧义为0，复制最大2186us；不再是全部UNKNOWN。精确CPU/Writer/IRQ指标仍待已构建的仪表化短窗，详见B2_CAMERA_SESSION_VALIDATION.md。旧SD-only49.5MHz/64KiB批量写与当前Camera+SD最多25MHz/多次小写不是matched-load，不以1809KiB/s推导当前吞吐保证。B1长期对照依用户跳过。

## B2最终实测（取代前文待补测状态）

R0000003的101帧、完整测量和PC回读已通过；真实SD时钟23.409MHz/1-bit，CSI分组240×94对应22560 B，CPU非idle任务占用估计39.9%，CSI/SD IRQ最大3/7us；write最大36.939ms、sync最大157.008ms，采样溢出与错误0，队列高水位33后正常排空。报告含24.75/49.5MHz历史SD-only观测对照，明确时钟/请求大小/预分配/计时口径差异，不作matched-load因果或长期保证。主线控制并行仍未验证。
