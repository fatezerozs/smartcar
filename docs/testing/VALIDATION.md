# 测试体系重构验收记录

日期：2026-11-02（Asia/Shanghai）。本页记录本轮工作区执行，历史验收不计入
通过数。任务书授权的性能数据集延期按接口处理，没有生成替代数据或批准基线。
各条 Git SHA、dirty 和“main 未变”均对应实际运行/验证当时，不是对后续 HEAD 的声明。
C32 工程提交与归档边界见 [C32 说明](../changes/2026-11-02_C32_TESTING_FRAMEWORK.md)。

## 最终执行

```powershell
./tools/testing/Invoke-TestPlan.ps1 -Profile PreFlash -Compiler gcc -Jobs 2
```

- Run ID：`20261101T175852-a5a26be988`（UTC 命名）。
- Git SHA：`79c8ec61bf5e5e78e48c860c20c53b4c836b9b58`；工作区 dirty，包括本轮改动与原有改动。
- 汇总：**PASS；57 个套件 PASS，1 个可选套件 SKIPPED**，用时 313.735 秒。
- 运行前后源码、夹具、工具及构建输入哈希一致。
- 121 次受管理进程调用均记录 `cleanupVerified=true`。
- `softwarePassed=true`，`releaseEligible=false`：本轮显式使用 GCC 补充 Host 证据。
- 工具：GCC 14.2.0、UV4 5.39.0.0、ARMCLANG 文件版本 18.0.0；完整命令及版本证据保存在环境和原始日志中。

完整本地证据：
[summary.md](../../.test-results/20261101T175852-a5a26be988/summary.md)、
[run.json](../../.test-results/20261101T175852-a5a26be988/run.json)、
[environment.json](../../.test-results/20261101T175852-a5a26be988/environment.json)。
可提交的精简索引为 [validation.json](validation.json)，包括结果文件哈希、套件
结果和九个 AXF 的 SHA-256。生成目录本身不提交；CI 按 artifact 归档。

## 已验证内容

| 内容 | 本轮证据 |
| --- | --- |
| 套件/路径/Schema/DAG | 66 个套件定义校验通过，含显式分组和人工占位 |
| 原有 C 场景 | 29 个独立套件、37 个编译变体通过 |
| 前后兼容 | 相同源码列表、宏、编译模式下，37 个变体的场景 stdout 完全相同 |
| 夹具拆分 | 10 个被修改 C 文件的场景主体在入口名归一化后逐字节一致 |
| 配置契约 | 6 种合法宏构型运行通过，6 种非法构型命中对应生产 `#error` |
| 接口/数据契约 | 真实公开结构字段、帧时戳和预览几何关系编译检查通过 |
| 离线工具/协议 | 7 个 Python 套件和 Field Protocol 通过；采集仅使用 Replay，串口恢复使用 FakePort |
| 框架自测 | 29 项通过，覆盖 Schema、越界路径、循环依赖、失败继续、阻塞、标准退出码、超时及后代清理、资源锁、指标门禁和噪声区间 |
| 目标/静态检查 | 原工程检查、全部工程输入引用、PWM 写入归属、fixture 哈希/尺寸、测试间源码包含约束通过 |
| ARM 源码 | 私有副本内多分支编译通过；不冒充完整链接 |
| Keil 完整链接 | base/train/route/switch/auto-track/link/threshold/motion/speed 九种构型均为 0 Error(s), 0 Warning(s)，AXF 新鲜且哈希记录 |
| 并行/隔离 | Jobs=2 完整计划通过；另在并发运行期间验证旧速度入口和 motion diagnostic 参数转发 |
| Diagnostic | 独立协议诊断通过，`releaseEligible=false` |

前后对照的完整哈希与检查数量见 [fixture-migration.json](fixture-migration.json)。
对照基线为 `20261101T173921-f7a2f30905` 中**拆分前的原始 C 场景**；该轮
新加框架/配置探针当时仍在调试，因此基线整轮汇总不是验收通过。本页只使用
其中实际 PASS 的 29 个原始 C 套件作前后对照，整体验收以最终 Run ID 为准。

## 状态与环境边界的反例验证

| 场景 | 结果 |
| --- | --- |
| 默认 MSVC 缺失，同时选择独立 fit-model | compiler-environment=BLOCKED；fit-model=PASS，说明无关模块未被阻塞 |
| GCC ASan 探针 | 链接器找不到 `-lasan`，compiler-environment=ERROR；Vision=BLOCKED，没有执行业务用例 |
| 显式选择 hardware-acceptance | 框架自测确认整轮 MANUAL_PENDING、softwarePassed=false；没有访问硬件 |
| 故意返回非零的合成子进程 | 框架自测确认 FAIL，依赖者 BLOCKED，独立套件继续 |
| 故意超时的父/子进程 | 框架自测确认 TIMEOUT、退出码 124；后代写入停止，清理确认通过 |
| 性能数据待接入 | performance-host=SKIPPED，其他必需套件完成且不依赖此节点 |
| 质量退化但运行更快的合成比较输入 | 比较器自测确认总门禁 FAIL，不能用速度抵消质量失败 |

环境反例报告：
[MSVC 缺失与独立模块](../../.test-results/20261101T175959-13d9a78134/summary.md)、
[ASan 缺失](../../.test-results/20261101T175959-164932a528/summary.md)。
旧包装兼容证据：
[Speed group](../../.test-results/20261101T175448-22d0d65f36/summary.md)、
[Motion diagnostic](../../.test-results/20261101T175512-60215ae2ad/summary.md)。

## 验收标准对应

| 编号 | 结论 |
| --- | --- |
| A1 | 完成：66 个原测试资产、23 个原工具的用途、归属、原始哈希及 C31 新增/扩展分类已记录 |
| A2 | 完成：独立编译/运行入口，旧分组是显式 aggregate |
| A3 | 完成：DAG 显式化，已去除测试源码之间的包含链 |
| A4 | 完成：Suite/Host/PreFlash/Diagnostic 共用清单与执行器 |
| A5 | 完成：七种状态及标准/原始退出码可区分，有实际反例自测 |
| A6 | 完成：Job Object 隔离、终止、进程句柄等待及清理验证 |
| A7 | 完成：套件/Variant 私有输出、目标工程副本、跨运行资源锁；Jobs=2 验证通过 |
| A8 | 完成：SHA、dirty、diff/输入哈希、宏、工具版本及原始进程证据 |
| A9 | 完成：原始场景与 37 个变体的前后输出一致，最终运行输入未变化 |
| A10 | 完成：算法/协议/接口/合法与非法配置各有独立证据 |
| A11 | 数据延期：版本/哈希/统计/比较接口已实现；真实两版本数据基准待接入 |
| A12 | 逻辑完成：独立质量门禁和噪声区间已自测；真实质量阈值及批准基线待数据接入 |
| A13 | Host CI 已验：同一入口在远程 MSVC optimized/ASan 均通过；远程 PreFlash 与专用 Keil Runner 仍待验证 |
| A14 | 完成软件边界：自动计划无 L5，全部目标动作限纯编译/链接，硬件证据单独待验收 |

## 保留范围和未验项

生产控制/视觉算法、既有安全断言、运行参数和部署标定没有因本次重构改写。
原有未提交工程与 `docs/changes/` 工作记录保留；Keil 构建只修改套件私有副本。
本轮测试重构没有修改全局配置、Skill 或持久 Agent 指令。远程验收阶段的提交和
推送发生于用户授权的隔离验证分支；后续 C32 main 工程归档另见其提交说明。

本机仍缺少 MSVC 和可用的 GCC ASan 库。下节另行记录了随后取得的远程 MSVC
/W4 /WX optimized 和 MSVC ASan 证据，不将本机 GCC 结果混入远程统计。

真实数据集、Ground Truth、测量 Adapter 和人工批准基线按用户要求延期。
比较器使用合成输入自测，不代表实际算法准确率或速度改善。真实 MCU 周期数、
栈/WCET、相机/DMA/编码器、PWM/LDOK、中断/闭环时序和实车赛道行为没有验证；
完整 ISR/DMA/临界区审查仍须独立人工证据。

## 远程 Host CI 补充验收（2026-11-02）

用户明确授权将本轮 65 个文件发布到公开仓库的独立验证分支后，使用隔离副本
创建并推送提交。原工作区的 main、已有未提交工程及历史工作记录没有提交或
推送；**该次验证时**远端 main 为 `79c8ec61bf5e5e78e48c860c20c53b4c836b9b58`。

- 验证分支：[`ci/testing-refactor-20261102`](https://github.com/fatezerozs/smartcar/tree/ci/testing-refactor-20261102)。
- 被测代码提交：`9be664d2bfb7b4bb136509538c8235f556519df0`。
- GitHub Actions：[`Host regression #1`](https://github.com/fatezerozs/smartcar/actions/runs/41389161428)，结论 **success**。
- 每组从 clean checkout 执行与本地相同的 Host Manifest/Runner，使用 MSVC Toolset 14.51.36231、Python 3.12。

| Job | 结果 | 套件统计 | 报告归档 |
| --- | --- | --- | --- |
| [MSVC optimized](https://github.com/fatezerozs/smartcar/actions/runs/41389161428/job/125096524918) | success | 43 PASS / 1 SKIPPED | [host-optimized](https://github.com/fatezerozs/smartcar/actions/runs/41389161428/artifacts/13116643131) |
| [MSVC ASan](https://github.com/fatezerozs/smartcar/actions/runs/41389161428/job/125096524865) | success | 43 PASS / 1 SKIPPED | [host-asan](https://github.com/fatezerozs/smartcar/actions/runs/41389161428/artifacts/13116608597) |

两组均通过 Manifest 校验、完整 Host 计划与归档步骤。每组 107 次受管理进程
调用的 `cleanupVerified` 均为 true，运行前后输入哈希一致；ASan 编译命令
实际包含 `/fsanitize=address`。两组唯一跳过项均为 `performance-host`，符合
数据集延期接口设计。框架自测各通过 29 项。

已下载两个 ZIP，按 GitHub 提供的 SHA-256 摘要核验后，仅提取 JSON 和日志，
未执行其中程序。归档中的完整 Host `run.json` 和套件结果通过当前 JSON Schema
复核；独立 Job 状态和原始日志同样显示 success。摘要记录在 `validation.json`
的 `remoteCI` 字段。

远程 PreFlash 尚未运行：它需要独立的、有正确 Keil/设备包/许可的 Windows
runner 和人工触发。本轮通过结论只覆盖远程 Host optimized/ASan，不包含远程
目标构建、真实数据性能、硬件验收或合并 main。随后补充的验收文档不改变上述
被测代码提交。

## 阅读和证据层级

- 原任务书：直接读取原始 Markdown；当前无 MarkItDown MCP，未使用 Markdown 转换结果冒充版面证据。
- `md_extracted`：没有仅据此最终认定的公式、表格数值、单位或测试结果。
- `structured_parsed`：JSON 清单/Schema/结果、工程 XML、Git 身份和工作区状态。
- `computed`：本轮执行、进程清理、输入/产物哈希、前后场景主体和 stdout 对照。
- `pdf_verified`：未使用；源是原生 Markdown/代码/结构化文件，不含待核验 PDF 版面。
- 剩余精度边界：Ground Truth/数据指标未接入，硬件时序未测量；没有据 Host 耗时推断 MCU 指标。
