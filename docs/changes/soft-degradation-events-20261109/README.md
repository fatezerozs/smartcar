# Phase3：near/full事件解耦

2026-11-09。依据[连续降级方案原文归档](SOFT_DEGRADATION_PLAN.md)（原路径为外层工作区 `.build/vision-replacement-research-20261109/regression-01/SOFT_DEGRADATION_PLAN.md`），完成连续降级方案的第一项适配层修改。本轮只修改当前工作区，未烧录、未改变板载V4、未接管原PI。

## 实际行为

- `demo/code/vision_metric_io.c`：full样本通过自身的实测双边、投影、连续性和标定域校验，即独立报告有效；near失败不再连带清零full。`domain_valid`仍表示旧near域准入，不能以full有效伪造near有效。
- near失败、full有效时，可使用已有的采集时刻里程插值；若没有可用插值，立即交付near失效事件，不为等待full对齐延迟零驱动。未对齐的full不能进入规划缓存。
- `demo/code/vision_planner_shadow.c`：独立保留经过时间/里程校验的full证据。监督器仍负责唯一运行准入，缺少near方向时保持原有零驱动恢复等待。重复帧不会刷新缓存身份。
- `tools/speed_planning/metric_input_audit.c`：删除将full接受强制改写成near依赖失败的旧诊断逻辑，与真实适配层一致。

本轮没有新增降速倍率、保护期限或第二个控制writer。没有删除既有near恢复期限、far预算、失帧/反馈故障和STOP优先级。短时near失效仍保留会话并允许按原条件恢复，期限耗尽仍可能锁存。可变采样、残差权重、剩余路径可停性和连续能力融合尚未实现，不能将本轮表述为完整软降级控制器。

## 验证

7个相关测试套件通过：planner-shadow、metric-control、planner-runtime、metric-odom-replay、planner-shadow-replay、phase3-shadow-preparation、vision-image-regression。新增断言覆盖near独有行缺失、共享行缺失、full独有行缺失、可选插值缺失、NaN候选隔离、重复帧不续命、恢复、STOP后不重启，以及影子near失效不撤销原PI授权。

- [核心专项报告](evidence/core/summary.md)
- [回放与图像报告](evidence/replay/summary.md)
- [补充真实运行模块模拟IO验证](evidence/runtime/summary.md)
- [750帧独立拒绝统计](evidence/audit/report.json)

当前配置下750帧near接受470帧、full接受376帧，与修改前一致。这次修正了事件依赖关系，没有提高该序列识别覆盖率；针对near失败/full独立有效的分支由专门构造的原生C测试覆盖。回放仍使用明确的静止合成编码器，仍存在长时间near失效后的锁存，不能证明实车跟踪或停车能力。

方案按原生Markdown全文读取；代码和JSON为structured_parsed，测试与回放为computed。没有md_extracted独有数值结论，也没有pdf_verified内容。未做目标ARM构建和实车验证。全仓git diff检查发现此前Keil项目文件已有尾随空白，本轮没有改写该文件；没有修改全局配置、技能或Agent行为。
