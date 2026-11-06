# Recorder / 标定分阶段任务首批交付（B1按用户跳过）

日期：2026-11-06。**P0审计与C0规程完成；B2静止短窗完整验收通过，B1按用户跳过；后续运动/生产阶段未放行。**

- [验收矩阵与下一批次边界](ACCEPTANCE.md)
- [P0基线与源码依赖](P0_BASELINE.md)
- [内存与DMA预算](MEMORY_AND_DMA_BUDGET.md)
- [数据契约缺口](DATA_CONTRACT_GAP.md)
- [SD构建输入/XML差异](BUILD_INPUT_DIFF.md)
- [无Recorder主线时序监测方案](CONTROL_TIMING_BASELINE_PLAN.md)
- [B1跳过记录](B1_SKIPPED.md)
- [B2候选、测试与使用说明](B2_CAMERA_SESSION_VALIDATION.md)
- [SCRAW-1协议/PC报告增补](SESSION_FORMAT.md)
- [SD/CSI争用测量方案](SD_CSI_CONTENTION_PLAN.md)
- [C0测量规程](C0_CALIBRATION_PROTOCOL.md)
- [C0原始测量模板](C0_MEASUREMENT_TEMPLATE.csv)
- [C0不确定度](C0_UNCERTAINTY.md)
- [待审核参数表](C0_PARAMETER_REVIEW.json)
- [变更与保护自审](evidence/final-review.json)

主线纯构建：`.build/recorder-camera-stages-20261106/primary-01`。
静止候选：`demo/recorder_b2/mdk/recorder_b2.uvprojx`；实际最终构建目录记录在本目录evidence/b2-candidate-location.txt，构建哈希见b2-metrics-v2-build-evidence.json；本轮对应镜像及编译时间差异见b2-tested-image-provenance.json。
保留的C生产者合成Session、完整/缺口/CRC损坏/截断四案例、全量PNG与Label Studio包在 `C:/shengchanli/smartCar/recorder_stage_data_20261106/`，来源与哈希见evidence/b2-demo-result.json。它们不是实板Camera数据。

复现：

```powershell
python -m unittest discover -s tests/recorder -p test_continuous.py -v
python -m unittest discover -s tests/vision_data -p test_pipeline.py -v
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_recorder_b2.ps1
```

没有修改主线C26或原训练工程。首批软件阶段没有硬件操作；之后经用户批准并由用户换烧，已经完成B2静止串口采集与只读导出，未移动底盘。原训练入口会初始化执行器且仍有默认0:盘号缺口，不能直接用于静止试验。B2授权短窗已完成；不能据此跳至B3/C1/D/E。

## B2最终真实交付

当前验收报告为[B2_CAMERA_SESSION_VALIDATION.md](B2_CAMERA_SESSION_VALIDATION.md)，联合审核[evidence/b2-final-acceptance.json](evidence/b2-final-acceptance.json)15项全部通过。R0000003实测101帧、约50.03FPS，CRC/序号/PNG/时间戳完整；CPU/Writer/IRQ/栈报告与Session对应。旧R0000002已补回读100帧完整，其旧测量故障仍保留为FAIL。所有三个Session原始301帧保留。

- 本轮镜像快照：[evidence/b2-tested-image-20261106/recorder_b2.axf](evidence/b2-tested-image-20261106/recorder_b2.axf)
- 最终文件与证据清单：[B2_FINAL_DELIVERY_MANIFEST.json](B2_FINAL_DELIVERY_MANIFEST.json)
- 实测数据根：C:/shengchanli/smartCar/b2_real_session_20261105T202916265Z/；含原始源、全部101张PNG、完整timeline、11条Label Studio任务、像素分析及预览。

本轮结束时CSI接收已停止，文件关闭卸载，COM14恢复115200并关闭。B2通过仅限批准的静止短窗；长期稳定性、主线控制并行及车辆运动不在本轮通过范围。
