# main合并后的主要文档同步

维护日期：2026-11-07，Asia/Shanghai。用户要求根据已有变更维护主要文档。本次只修改说明与证据索引，当前功能基准是已经合入main的`00bb7a47ee5b15fe845137895316d54ef24f6a1d`。

[工程导航](../PROJECT_INDEX.md) · [项目现状](../PROJECT_EVOLUTION.md) · [测试验收入口](../testing/VALIDATION.md) · [机器记录](2026-11-07_MAIN_DOCUMENTATION_SYNC.json)

## 1. main现在包含什么

| 对象 | 已核定的状态 |
| --- | --- |
| 合并来源 | `feat/vision-recorder-training`，集成头提交`6b285d3fbef058f29c6890e16ee277e327aeb2f7` |
| main合并提交 | `00bb7a47ee5b15fe845137895316d54ef24f6a1d`；保留main和开发分支历史 |
| 内容基准 | 文件树`375fc0f87d8883ddee6521b61fd0f4ae580e3f50`，与审核归档`16baa6b`完全一致；按用户指示不保留空文件`1103` |
| 主工程 | `demo/mdk/competition/MPC_20261106.uvprojx`；维护日期变更不重命名工程 |
| 构型关系 | MPC/C26主工程、AUTO_TRACK、RecorderTrain、B2/B3和联合候选保持各自构建、宏、授权和验收边界 |
| PR与推送方式 | 连接器创建PR返回403；本次合并已改用普通Git合并提交和非强制推送完成，不是PR已创建或账号整体无权限的证明 |

main已包含MPC阶段源码、Recorder与PC数据链、标定资料、SD独立工程及测试扩展。源码进入同一分支，不表示主赛道工程启用了Recorder的RTOS/SD，也不表示所有构型已联合验收。另一份本地main验证工作树当时保留原状，GitHub main与该工作树不能只凭同名视为同一SHA。

## 2. 已有验证与对应身份

| 记录 | 结果 | 适用范围 |
| --- | --- | --- |
| [main CI，workflow 42243602219](https://github.com/fatezerozs/smartcar/actions/runs/42243602219) | optimized与ASan各49 PASS / 1 SKIPPED | main `00bb7a4`，两项任务均completed/success |
| [合并前分支CI，workflow 42214055736](https://github.com/fatezerozs/smartcar/actions/runs/42214055736) | 两模式各49 PASS / 1 SKIPPED | 功能提交`7d2ff6d`；与main运行分别归档 |
| 完整本地PreFlash | `20261106T114440-8e894432c2`：63 PASS / 1 SKIPPED | GCC补充Host、ARM编译、九种Keil构型；后续生产源码未变化 |
| 后续测试专属修正 | `20261106T115628-c46e55a1e3`：8 PASS | Recorder reviewed比较及组合回放文件打开的兼容性修正；完整MSVC由后续分支/main CI覆盖 |
| 当前清单 | 72个定义；Host/PreFlash/Diagnostic闭包50/64/48个 | 由版本化catalog与依赖选择逻辑复核，不把不同suite内的检查次数相加 |

main CI于北京时间2026-11-06 23:30:03创建，23:34:56更新为完成；原API时间为UTC，机器记录保留原字段。两个任务ID分别为optimized `127979965194`、ASan `127979966183`。唯一跳过项是缺版本化数据集、标签、测量Adapter和批准基线的`performance-host`，不是已测出算法性能收益。

MSVC Host报告的框架`releaseEligible`字段不代替动态标定、完整PreFlash人工风险审查、烧录决定或实车验收。前两轮MSVC失败及更正继续保存在[原CI兼容性证据](2026-11-06_MPC_CI_COMPATIBILITY.json)，旧Phase 3的46 PASS/3 FAIL/1 SKIPPED不改写成一次新运行。

## 3. 本次维护的阅读口径

| 文档组 | 修正重点 |
| --- | --- |
| README、文档目录、工程导航、演进档案 | 从开发分支归档切换为已合入main；区分工程名、框架版本、固件身份和维护日期；补当前CI及后续任务 |
| 测试手册、测试架构、验收入口 | 当前72定义和main结果前置；66定义、57/43等原C32数字继续保留在历史时点 |
| Keil与测试入口 | 明确共享仓库中的独立构型；生成工程从脚本重建，固定AXF不自动升级 |
| Recorder架构/实施/问题/验收计划 | 关闭“源码未提交/未发布”；区分源代码合入与主构型启用；保留预算未集成和运动时序门 |
| PC数据链 | SCRAW-1 adapter已有实现；区分主链synthetic/mock、B2独立真实短窗回读、真人审核与真实运动Dataset Release |
| 规划与Encoder参考 | 当前左右CPR已采用；早期1024/4096或未知CPR按历史诊断理解；主线预算接入仍未完成 |
| SD使用说明 | 已有独立实板与断电后PC校验，替换“尚无硬件结果”的当前表述，保留原构建记录和具体实测限制 |

既有关系图仍使用实线表示复用、虚线表示待集成。问题表按关闭条件维护，不使用百分比推断验收；冻结目录内的报告、原始测量和清单保持原字节。持续维护页可补充更新提示及调整标题层级，历史结果数字不改写。

## 4. 仍需推进的关键项

1. Phase 2：左右负载滚动周长、实际轮角、相机地面映射/盲区、速度响应/制动/总延迟与同步运动Session。左2335.8、右2358.6 counts/轮及前进sign−1/+1已采用，不再列作未知；重复性限制仍保留。
2. Phase 3：MPC、AUTO_TRACK、RecorderTrain共用预算规划/监督接口，各自保留启动与调度边界；一致快照、候选复核、短提交区和受控停止仍未完成。当前零动力恢复是过渡实现。
3. 联合与运动：B2静止101帧、B3强制零实际输出及独立SD通过不能相加成为联合或运动PASS；仍缺对应镜像的联合静止、正常运行到OFF时序、SD并行长尾、稳定运动回读和赛道验收。
4. 数据集：补真人审核、完整Session切分/留出、批准GT/Adapter/基线和真实同输入A/B；synthetic演示不提供真实准确率或性能收益。

## 5. 校验与来源

本次核对Git提交/文件树、GitHub workflow与job状态、原生Markdown、catalog/参数JSON、生产宏及既有测量报告。文档校验包括发布链接/新增锚点、Markdown表格列数、72项清单对照、文档命令的已有接口、仅文档差异、原始交付哈希及Encoder数据复核；机器记录给出实际完成结果。未重跑无改动的固件构建，未新增串口、烧录、复位或实车测试。

读取层：原生Markdown直接阅读；Git/JSON/XML/B3T1为`structured_parsed`；CI结果计数、依赖闭包、CRC/模差/CPR与哈希为`computed`。当前无MarkItDown MCP，未以`md_extracted`单独认定精确数值；没有新增`pdf_verified`或几何量测。标称/图像估算、操作者圈数和已记录重复性差异仍是参数精度限制。没有修改全局配置、技能或持久Agent行为。
