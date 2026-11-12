# V10选定软件证据与V9观察

- [V9串口统计](serial-summary.json)：2373条完整记录，876条ON非零目标且obs均0；时间回退按段保留，不证明唯一现场根因。
- [视频身份](video.json)：仅文件名、时长与SHA，不上传原视频或重建车载灰度。
- [修复前输出](before.log) · [历史最小夹具](before.c) · [V10差异](V10-vs-V9.patch)。历史夹具在原电脑冻结V9根运行；不是本版的通过断言。
- [修复后与集成检查](swing_brick.log) · [环岛状态检查](ring_transition.log) · [原兼容](compat.log) · [单边居中](edge.log) · [双边居中](centering.log) · [控制/PI](tuning.log) · [S弯](s-turn.log) · [Keil](keil.log)。
- [受保护源码核对](protected-v9.json)。

这些是已执行的软件检查及V9历史观察，不是V10实车通过证据。原始UART/视频及含历史录制的原包继续在原电脑保存，当前上传未运行车辆。
