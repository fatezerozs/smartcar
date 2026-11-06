# 主文档数据入口核对与规划接入 v0.3

日期：2026-11-06，Asia/Shanghai。当前入口为主页 README → [当前参数集与实测数据交付](../vehicle-parameters-20261106/README.md)；持续现状由 [项目演进档案](../../PROJECT_EVOLUTION.md) 引用该交付。本页记录规划模块如何消费数据，以及本次实际复核范围。原交付、原始测量、历史 profile 和历史证据均保持原样。

## 1. 数据分层与可用范围

| 数据入口 | 本轮读入与核对 | 对规划任务的作用 / 仍缺什么 |
| --- | --- | --- |
| [生产 Encoder 记录](../../../profiles/encoder-production-20261106.json)、[源码配置](../../../demo/code/midterm_route_profile.h) | 左 2335.8、右 2358.6 counts/轮，前进 raw 符号 −1/+1；与生产宏、10 圈记录及交付快照一致 | 可替代旧 1024 假设；仍无负载滚动周长，不能给出已标定 m/s 或 m |
| [原始窗口与台账](../vehicle-parameters-20261106/evidence/encoder-measurement-ledger.json) | 9 个 B3T1、4500 次 10ms 采样、7 组手转比值；保留排除组、其他轮计数变化、重复性未通过字段 | 原生计数链与方向依据；窗口位于手转前后，不能重建未记录的手转速度轨迹 |
| [B2 Camera/SD 验收快照](../vehicle-parameters-20261106/evidence/b2-final-acceptance.json) | 101 帧、约 2 秒、独立静止 Camera/SD 窗口；本轮仅核验交付快照完整性 | 有真实图像入口；未在本轮重读完整图像 Session，不能把 B2 与不同 Session 的 B3 计数拼接 |
| [几何 v0.1](../../../profiles/c1-host-geometry-v0.1.json) | 64mm 标称轮径、205mm 估算轴距、152/150mm 估算轮距；沿用原图来源 | 支持显式参考计算；不是负载滚动周长、相机地面映射或实测尺寸 |
| [舵机资料](../../reference/C1_VEHICLE_MODEL.md) | 1430/1550/1670µs 静态命令端点 | 没有脉宽→实际前轮角映射，不能启用经过物理标定的几何转弯补偿 |
| [联合候选构建快照](../vehicle-parameters-20261106/evidence/joint-build-evidence.json) | 读取既有构建结果，校验归档哈希 | 不是本轮构建或联合板端通过；联合候选的后续状态见原交付入口 |
| [控制 Session 契约](DESIGN.md) | 已有 `smartcar.control-replay/1` 与完整 Session 留出规则 | 尚无符合该契约、经审核且同步的运动图像/near/far/编码器/输出数据；不生成 far-loss 统计预算 |

这里的 CPR 已经按用户指令采用为生产参数。本轮尊重这一结论，同时保留原记录 `metrological_repeatability_passed=false`；不把采用值降回“1024 待猜”，也不把采用行为写成重复性认证。

## 2. 本次校验程序与拒收条件

[data_inputs.py](../../../tools/speed_planning/data_inputs.py) 为离线入口，复用现有 B3T1 decoder 的纯 `decode`，不调用其写文件或串口采集入口。实际复核：

1. 校验交付 MANIFEST 所列 70 个文件的字节数和 SHA256，拒绝重复/越界路径；必要输入必须列入清单。
2. 对 9 个原始 trace 重新核对 CRC、64 字节事件结构、ordinal、格式与真实来源标记；要求完整窗口、C26 OFF、三执行器快照齐全且观测实际 duty 为零。
3. 按 SAMPLE.v11 关联同次 RAW 事件，核对时间、相邻采样序号、10ms 间隔及 16 位模差；核对累计计数差与 counts/10ms，拒绝饱和、半域歧义和内部矛盾。
4. 用窗口末累计计数重算 7 组左右增量、16 位余数、每圈比值及方向。人工整圈数、方向和无中途复位仍是操作者证据；CRC 不证明人工圈数准确。
5. 核对两条采用记录、交付快照、当前生产 JSON 与源码 CPR_X10/方向宏；不同版本不得默默混合，漂移时明确失败。

这里不要求每个窗口绝对静止，原台账中另一轮跳变仍如实保留；未以丢弃异常组的方法制造重复性通过。报告将 `measuredMetricSpeed`、`motionNearFarBudgetDataset` 和 `productionPlannerIntegrationAccepted` 保持 false。

```powershell
# 仓库根目录；只读输入，报告写入被忽略的本地构建目录。
python -m tools.speed_planning.data_inputs `
  --output .build/data-inputs-20261106/data-input-audit.json
```

## 3. 新参考参数 v0.3 与左右轮尺度

[c1-host-reference-v0.3.json](../../../profiles/c1-host-reference-v0.3.json) 成为联动回放默认参数入口，引用当前生产 Encoder JSON。原 v0.2 的 1024 假设保留为历史场景，不能覆盖当前生产 CPR。两者不能同时作为一次回放的 Encoder 来源；工具拒绝这种冲突。

v0.3 仍是 `productionApproved=false` 的 Host 参考，速度上限、制动、jerk、延迟、near/full 米制几何、far 时距预算及假想电机模型均沿用参考假设，没有新增实测依据。

仅在该参考场景中使用：

\[
C_{nominal}=\pi\times0.064=0.20106192982974677\;m,\quad
q_L=2335.8/C_{nominal}=11617.316127314036\;counts/m,
\]
\[
q_R=2358.6/C_{nominal}=11730.71402426701\;counts/m.
\]

上面的米制尺度是“已采用 CPR + 标称周长”的组合参考，**不是地面实测速度/里程标定**。原几何 v0.1 中 CPR 的历史 null 与滚动周长的 null 不改写；新工具从生产记录获取 CPR，实际滚动周长继续待测。

联动回放保持共同纵向速度参考，分别生成左右 `round(v_ref × q_side × 0.01)` counts/10ms，再调用同一份现有 `wheel_speed.c` PI。左右 count 目标可能不同，这反映计数尺度不同，不代表用后轮差速替代舵机转向。增加断言，要求各轮目标量化误差不超过半个 count；不能让“左轮/右轮共用同一尺度”的错误被同值假数据掩盖。

比例 .5/1/2 仍同时改变模拟传感器与控制器尺度，仅研究量化；不是生产 CPR 改动、CPR 测量置信区间或标定错误试验。所有模拟速度、跟踪误差和停车位置均保持仿真性质。

```powershell
pwsh -NoProfile -File tools/testing/Invoke-TestPlan.ps1 `
  -Suite speed-planning-host -Compiler gcc -Jobs 2

# 显式重放旧参数；参数历史保留，代码使用本次版本，不承诺旧二进制逐字节重现。
python tools/speed_planning/replay.py `
  --reference profiles/c1-host-reference-v0.2.json `
  --output .build/data-inputs-20261106/legacy-replay
```

回放报告升级为 `smartcar.composed-reference-replay/2`，记录选定 profile 哈希、生产 Encoder 哈希、左右尺度、`measuredMetricScale=false`；分辨率字段为 `sensorResolutionFactor`。历史报告 v1 保留，不重写。

## 4. 实际验证与发现的覆盖缺口

首次相关回归 `20261106T072730-c01f851343`：environment、compiler、speed-planning-host、c26-baseline-characterization、encoder-board、speed、speed-group 共 7 节点 PASS。之后只增加规划断言和离线校验，不修改生产模块。

最终规划回归 `20261106T072920-91db4b22cd`：environment、compiler、speed-planning-host 共 3 节点 PASS，23 个 Python 用例；原 C 规划检查 104496 次，其中 100000 次为固定输入重复；联动回放 12 场景 × 3 分辨率 × 2 存储消费速度，共 72 次场景执行、18000 个快速消费采样。检查次数不是独立场景数。

新增拒收覆盖：文件哈希变化、目录越界、CRC 损坏、重新计算 CRC 后仍存在的 RAW/累计计数矛盾、生产源码 CPR 漂移以及 Encoder 来源冲突。测试复用真实原始输入，并在临时目录构造损坏副本，不修改原始数据。

接入新分辨率后发现，原名为“far-time-budget”的场景可能先达到 0.24m 距离上限。现在该场景显式将距离预算设为 1m，只隔离验证 600ms 时间到期；距离场景设 2000ms/0.05m，隔离验证距离先到期。两者都检查首次 `CONTROLLED_STOP` 的触发条件，覆盖“任一先到”而非仅检查最终停车。其他场景仍使用 v0.3 默认参考预算。测试覆盖修正不是生产策略参数修改。

快速/延迟遥测消费的每步控制快照哈希仍一致；near-invalid、STOP、失效候选提交、相机/Encoder 超时及过急限速继续通过。报告的 GCC 软件范围 `softwarePassed=true`、`releaseEligible=false`。本轮没有重跑无改动的主 Keil 工程、远程 CI、MSVC/ASan 或 ARM 对象编译；此前结果不作为本轮结果计数。当前工作区实际分支见测试报告，为 `feat/vision-recorder-training`，未切换分支或提交。

因子1的假想模型中，直线/弯道活动稳态跟踪RMSE分别约0.003713/0.014751m/s；不是实车误差，也不能用来宣称新参数提高了物理性能。时间预算场景首次失效1000ms、控制停车1600ms、停稳锁止2470ms；距离场景首次失效1000ms、控制停车1130ms、锁止2020ms。时间预算场景有2个`brakeOverride`采样，保留在原始CSV/报告，表示硬限速优先于正常jerk整形，不能把整段描述为无例外的平滑轨迹。

可携带摘要见 [data-input-evidence.json](data-input-evidence.json)，完整日志位于 `.test-results/<runId>`，输入复核和本轮文件差异位于 `.build/data-inputs-20261106`。

## 5. 下一步及硬件边界

本次不再把“是否有轮端 CPR”列为阻塞项，也不重复申请已由其他工作取得数据的同一 C0 测试。[原 C0 申请](TARGET_TEST_REQUEST.md) 已注明由新证据取代；本任务没有执行其串口步骤。

接下来需要的输入是左右负载滚动周长、脉宽到实际前轮角、相机地面映射/盲区及同步运动 Session。动态制动/延迟与 near/far 可信质量标签仍未取得。B2 的 SD 调用延迟不能代替 capture→control→PWM 总延迟，静止 PI/输出证据不能代替负载轮速和制动验收。

可用参考值继续做 Host 设计；进入主构型的物理速度、制动与降级预算仍须对照这些缺项审查。后续最小固件接入、C26 重新授权/故障分类修正及主路径最终零输出封口尚未完成。本次没有访问串口、烧录、复位或恢复动力。

## 6. 读取层与准确性边界

原生 C/Markdown 直接读取；JSON/二进制 B3T1 是 `structured_parsed`，CRC/SHA256/模差/CPR/尺度及测试是 `computed`。本次数值不依赖 `md_extracted`，未使用不可用的 MarkItDown，也无新 `pdf_verified` 内容。几何图片沿用既有原图核验来源，本轮不重新估算。剩余精度风险是操作者圈数、已记录的重复性差异、缺失负载周长/真实轮角/地面标定及静止与运动场景分布差异。
