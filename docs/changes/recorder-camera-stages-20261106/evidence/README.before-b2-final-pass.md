# Recorder / 标定分阶段任务首批交付（B1按用户跳过）

日期：2026-11-06。**P0审计、B2静止候选与PC准备、C0规程完成；真实B2与后续阶段未验收。**

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
静止候选：`demo/recorder_b2/mdk/recorder_b2.uvprojx`；实际最终构建目录记录在本目录evidence/b2-candidate-location.txt，验收哈希见b2-build-evidence.json。
保留的C生产者合成Session、完整/缺口/CRC损坏/截断四案例、全量PNG与Label Studio包在 `C:/shengchanli/smartCar/recorder_stage_data_20261106/`，来源与哈希见evidence/b2-demo-result.json。它们不是实板Camera数据。

复现：

```powershell
python -m unittest discover -s tests/recorder -p test_continuous.py -v
python -m unittest discover -s tests/vision_data -p test_pipeline.py -v
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build_recorder_b2.ps1
```

没有修改主线C26或原训练工程，未烧录/串口通信/移动底盘。原训练入口会初始化执行器且仍有默认0:盘号缺口，不能直接用于静止试验。下一阶段必须单独批准候选烧录与静止Camera采集，不能跳至B3/C1/D/E。
