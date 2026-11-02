# C32：测试体系重构、性能回归接口与 CI 验收

提交日期：2026-11-02（Asia/Shanghai）。提交分支：`main`。
代码基准：`79c8ec61bf5e5e78e48c860c20c53b4c836b9b58`（C31 软件架构硬化）。
已经验证的测试体系快照：`9be664d2bfb7b4bb136509538c8235f556519df0`。

**C32 是仓库工程与测试体系版本，不是新的车载控制算法或已批准的硬件发布。**
生产固件仍保留 switch 的 `MIDTERM_C26_31` 和 auto-track 的
`MIDTERM_C31_AUTO_TRACK_1` 标识；协议白名单、控制参数、标定值与启动 review
门槛均沿用 C31。本次不把版本号替换成协议尚不接受的 C32 固件标识。

当前状态统一维护于 [项目设计与演进档案](../PROJECT_EVOLUTION.md)。
操作说明见 [测试体系](../TESTING_ARCHITECTURE.md)，精确结果索引见
[验收记录](../testing/VALIDATION.md) 和 [validation.json](../testing/validation.json)。

## 1. 本次提交的结论

测试体系的功能重构与自动软件验证已完成，可以作为 C32 工程提交归档。
这与“已完成正式烧录放行”分别记录：本地完整 PreFlash 使用 GCC，报告为
`softwarePassed=true`、`releaseEligible=false`；远端完成的是 MSVC Host
optimized/ASan。尚无完整 MSVC PreFlash 的统一放行记录，人工静态审查和真实
硬件风险也没有因此关闭。

| 结论层次 | 本次状态 | 适用边界 |
| --- | --- | --- |
| 测试体系实现 | 完成 | 统一清单、执行器、编排、隔离、兼容入口及结构化报告 |
| 本地 PreFlash 自动软件验证 | PASS：57 个套件通过、1 个可选接口跳过 | GCC Host + 自动静态检查 + ARM 编译 + 九种 Keil 纯构建 |
| 远程 MSVC Host | optimized 与 ASan 均 success，各 43 PASS / 1 SKIPPED | 与本地同一 Manifest/Runner；不包含远端 ARM/Keil |
| 数据驱动性能/质量 | 接口与比较逻辑完成，真实数据验证延期 | 没有数据集、Ground Truth、实际测量结果或批准基线 |
| 正式烧录放行 | 未闭环 | 不合并各报告布尔值冒充统一放行；人工审查与硬件风险继续保留 |
| 远程 PreFlash | 未执行 | 需要有正确设备包和许可的专用 Keil Windows runner |
| 新的板端/实车结论 | 无 | 产品目标 R1–R4、DMA/时间戳/IRQ/PWM 风险没有升级为完成 |

提交整理阶段主要进行源码身份、证据、文档链接和 Git 范围检查；下文标明每项
运行的实际来源，不把已有结果冒充提交整理时重新运行的测试。提交后的 main
CI 与被测提交 SHA 另以实际运行记录补充，不预写通过结论。

## 2. 为什么重构

旧体系由十个 CMD 入口和另外调用的离线测试组成。各脚本重复管理 MSVC、宏、
编译参数、输出、错误分支和退出码；部分测试通过重命名 main 并包含其他测试
源码来复用模拟模型。共用 `tests/vision/output/` 也限制了安全并行和证据归属。

C32 将测试身份、证明目标、执行机制和结果存储分开：

```text
PowerShell / 兼容 CMD / GitHub Actions
  → Suite JSON + Schema / 路径 / DAG 校验
  → Profile 选择 + 显式前置依赖 + 资源锁
  → 独立进程与套件/Variant 私有目录
  → 原始日志 + 结构化 JSON
  → 从 JSON 生成 Markdown
```

迁移前资产及历史依赖见 [盘点说明](../testing/INVENTORY.md) 和
[原始清单](../testing/inventory-before.json)。清单保存 66 个原有测试资产及
23 个原工具的用途、归属与哈希，并依据 C31 提交文件状态区分新增和扩展项。
带参数的 `replay_preview.c` 保留为诊断工具，不伪装成自验证测试。

## 3. 实现与文件归属

| 文件/目录 | 变化与职责 |
| --- | --- |
| `tests/suites/catalog.json` | 66 个稳定套件定义；源码、宏、Variant、前置依赖、超时、资源、Profile、证据类型集中维护 |
| `tests/schemas/` | suite/result/benchmark 三份 draft-7 JSON Schema；PowerShell Test-Json 实际校验 |
| `tools/testing/common.py` | JSON、路径范围、唯一身份、入口类型、DAG 与 Profile 闭包校验 |
| `tools/testing/process_runner.py` | 独立进程、Windows Job Object、超时与后代清理、跨运行 OS 文件锁 |
| `tools/testing/providers.py` | 工具链探针、C/离线脚本/静态/ARM/Keil/基准占位等执行适配 |
| `tools/testing/run_tests.py` | 套件调度、失败传播、状态/退出码规范化、输入身份记录 |
| `tools/testing/reporter.py` | 从持久化 run.json 生成报告，不靠关键字猜测业务结果 |
| `tools/testing/*.ps1` / `Test.Common.psm1` | Windows 公开入口、单套件、环境、清单、报告及基准命令 |
| `tests/support/fixtures/` | 五个公共模拟夹具：速度 IO、速度辅助、车道、开关、运行时 IO |
| `tests/vision/run*.cmd` | 十个旧入口继续存在，改为统一执行器的薄包装 |
| `tests/vision/test_*.c` | 十个相关文件改为显式包含公共夹具；场景主体与断言保持 |
| `tests/suites/contract/` | 公开数据结构及真实生产宏约束的正向/负向编译探针 |
| `tests/framework/test_framework.py` | 29 项执行基础设施、隔离、状态和指标门禁自测 |
| `tests/benchmarks/` | 版本/哈希/标签/批准基线契约、待接入数据接口与说明 |
| `.github/workflows/` | Windows MSVC optimized/ASan；单独的人工触发 PreFlash 工作流 |
| `.gitignore` | 忽略 `.test-results/`；日志、临时工程及固件产物不进入源码提交 |
| `docs/`、`README.md` | 更新当前工程版本、阅读入口、实现说明与证据边界 |

没有改写 `demo/code/`、`demo/user/`、`libraries/` 的生产行为或参数；没有新增
`releases/C32` 固件归档，也没有把历史 AXF 改名成 C32 发布件。原有未提交
`demo/mdk/competition/midterm_switch.uvprojx` 与 C31 工作记录单独保留，不夹带
进本次提交。提交前后按文件哈希检查这些既有工作内容。

## 4. 分层、Profile 与门禁

| 层次 | 证明内容 |
| --- | --- |
| L0 Environment | 操作系统、解释器、编译器探针、目标工具与必要输入 |
| L1 Unit | 局部算法、控制模块、板级 Host 适配及离线工具 |
| L2 Integration | 真实生产模块组合的运行时、路由、启动、速度及输出链路模拟 |
| L3-A Contract | 视觉分层 Oracle、协议、接口数据、合法配置与明确拒绝非法配置 |
| L3-B Performance & Quality | 固定数据和批准基线上的质量、计算、系统指标；本轮真实数据延期 |
| L4 Build & Static | 工程引用、自动静态约束、ARM 源码及隔离 Keil 完整链接 |
| L5 Hardware Acceptance | 独立人工/硬件证据，不进入自动 Profile |

Suite 只执行指定目标及真正前置依赖；Host 包含当前可自动运行的 Host 套件；
PreFlash 在 Host 上增加 L4；Diagnostic 收集指定诊断但不作为放行。`core`、
`switch` 等 aggregate 仅归并旧分组，不共享初始化状态。

状态区分 PASS、FAIL、ERROR、TIMEOUT、BLOCKED、SKIPPED、MANUAL_PENDING。
必需项非 PASS 时整体不能通过；无依赖套件继续执行，失败依赖的下游不运行。
套件标准退出码分别保留，原始进程退出码另存。非法宏测试必须命中对应生产
`#error`，不能把任意非零编译返回当作通过。

显式选择可选占位套件时，该目标在本次调用中变成必需项。默认串行；显式并行
最多 4，本轮完整验证使用 Jobs=2。Keil 和 ARM 等共享工具按资源锁串行，MSVC
ASan 额外保护 PDB 服务；套件输出、临时目录及每个 Variant 分开。

Windows 子进程在挂起状态加入 Job Object 后才恢复执行。超时和正常结束均
清理未回收后代，等待进程句柄并检查活跃进程归零。自测实际创建父子进程和
持续写入探针验证清理，不只检查模拟返回码。

`releaseEligible` 只适用于该报告记录的 Profile 和工具链，不是硬件操作授权。
Host 的 true 不包含 L4；GCC PreFlash 的 false 也不应被改写成业务失败。本轮
保留原始状态，没有为归档 C32 而修改判定或强制设置放行。

## 5. 兼容性与本次验证证据

原有测试继续链接真实生产模块。共享对象为模拟 IO/辅助模型，不是其他测试
的场景 main；移除 `.c` 包含链后，每个 EXE 拥有自己的状态与重置入口。
stubs 和历史 reference 保留原址；参考算法独立重命名编译，不加入 MCU 工程。

前后对照覆盖 29 个原有 C 套件、37 个编译变体：源码列表和宏一致，场景 stdout
一致；十个被拆分文件的场景主体在入口名归一化后逐字节相同。详见
[fixture-migration.json](../testing/fixture-migration.json)。计数包含逐像素检查，
不把检查总数包装成独立场景数。

| 来源 | 精确身份与结果 | 证明内容 |
| --- | --- | --- |
| 拆分前 C 对照 | `20261101T173921-f7a2f30905` 的 29 个原始 C 套件 PASS | 只作场景对照；该轮新增探针仍调试，整轮不是验收通过 |
| 本地完整 PreFlash | `20261101T175852-a5a26be988`；57 PASS / 1 SKIPPED；121 次受管进程清理确认 | GCC Host、自动静态、ARM 及九种 Keil 构型；运行前后输入一致 |
| 远程 MSVC optimized | [job 125096524918](https://github.com/fatezerozs/smartcar/actions/runs/41389161428/job/125096524918)；43 PASS / 1 SKIPPED | C11、/W4 /WX、/O2；29 项框架自测 |
| 远程 MSVC ASan | [job 125096524865](https://github.com/fatezerozs/smartcar/actions/runs/41389161428/job/125096524865)；43 PASS / 1 SKIPPED | 52 次编译均实际使用 /fsanitize=address；29 项框架自测 |
| C32 编码归一后的定点复核 | `20261102T072026-4794159999`；6 PASS，无跳过/失败 | auto-track 两个 Variant 与 environment/compiler/vision/control/speed 前置依赖，GCC 证据单独记录 |

两组远程结果均来自 clean checkout 的 `9be664d2bfb7b4bb136509538c8235f556519df0`，
每组 107 次受管进程清理通过，run.json 与套件 JSON 一致并通过 Schema 复核。
MSVC Toolset 为 14.51.36231，Python 为 3.12。ZIP 按 GitHub 的 SHA-256 摘要
校验后只读取 JSON/日志；没有执行归档内的 EXE。

| 远程归档 | SHA-256 |
| --- | --- |
| [optimized / 13116643131](https://github.com/fatezerozs/smartcar/actions/runs/41389161428/artifacts/13116643131) | `671242310d5704f01eda146b61244e246fa0e928bb306ba0ce3c978345872382` |
| [ASan / 13116608597](https://github.com/fatezerozs/smartcar/actions/runs/41389161428/artifacts/13116608597) | `8c90bafaee4bd0420b91b2e7b45d0c6f2e94abca581363238154ffd7823be842` |

九种 Keil 构型为 base、train、route、switch、auto-track、link、threshold、motion、
speed，均为 0 Error(s), 0 Warning(s)。AXF 的逐项哈希保存在 `validation.json`；
当前产物与记录一致。目标编译在私有副本运行，未覆盖已有工程或发布目录。

提交前复核时，本地 686 个测试/构建输入与上述本地验证快照一致。为了使提交
源码与已通过的 CI Git blob 完全一致，`test_auto_track.c` 仅去掉 UTF-8 BOM；
原有换行/正文逻辑不变，并补跑 auto-track 及其依赖。提交暂存区另与 CI 快照
比较执行代码和生产源码，文档差异单独列出，不把 BOM/换行身份处理误称算法修改。
本次暂存范围固定为 68 个文件；既有工程与 C31 工作记录的 27 份内容指纹
保持不变，Markdown 本地链接、Schema/DAG 和 Git whitespace 检查均通过。

## 6. 数据集延期与性能接口

本轮没有声称真实准确率或处理速度改善。已有 94×60 静态预览仍是未标注、
下采样的功能 fixture，不作为完整分辨率 Ground Truth，也不构成真实控制时序。

`performance-host` 为可选 pending，输出 SKIPPED，没有功能/契约/构建套件
依赖它，因此不会因缺少数据集阻塞其他模块。真实接入仍需要：

1. 不可静默替换的数据/标签版本、来源、场景与 SHA-256；
2. 明确的离线测量 Adapter、参数、预热、重复次数和有效样本规则；
3. 指标单位、方向、统计方法和阈值依据；
4. 人工批准并保留历史的 clean SHA 基线；
5. 质量、计算、系统表现分别比较，安全相关质量不由速度抵消。

比较器已校验同一数据/标签/参数/编译器/环境/模式及样本集合，支持 mean、median、
P95/P99 等统计、绝对/相对退化/不退化/观察门禁。差异落入声明噪声区间或缺少
可比批准基线时为 INCONCLUSIVE。噪声带不是自动统计置信区间，合成自测也不是
实际算法性能证据。接口详情见 [benchmarks README](../../tests/benchmarks/README.md)。

## 7. 未关闭项与后续工作

| 事项 | 仍缺的证据或工作 |
| --- | --- |
| 统一 PreFlash 放行记录 | 当前完整报告使用 GCC；远端只有 Host，不能拼接布尔值宣称完整 MSVC PreFlash 已放行 |
| DMA 缓冲所有权 | Host 换帧/ABA 用例不能证明 DMA 独占、复制时间和回收时序；沿用 C31 未关闭风险 |
| 采集时间戳 | 完成回调的读取时间不等于曝光/硬件完成时刻；延迟 ISR、缓存/可见性及漏中断需目标证据 |
| IRQ、临界区与 PWM/LDOK | 自动写入归属检查不能替代最终反汇编、时序、同步更新、最坏执行时间与停车响应审查 |
| 远程 PreFlash | 专用 Keil runner、许可、设备包和人工触发实际运行尚未取证 |
| 数据驱动性能/质量 | 数据、标签、测量 Adapter、批准基线及真实阈值延期 |
| 实车目标 | 入环、卡顿、十字、上电准入等 R1–R4 仍按演进档案推进，无新增实车完成结论 |

本次提交不执行烧录、串口、相机/编码器访问、车辆运动或真实执行器输出；也不
将硬件验收接入普通自动 CI。后续实际烧录/硬件验收需要其独立任务与证据。

## 8. 重现与诊断入口

从仓库根目录（`C:\shengchanli\smartCar\smartcar`）运行：

```powershell
./tools/testing/Invoke-TestPlan.ps1 -List
./tools/testing/Test-Manifest.ps1
# 有 MSVC C++/ASan 环境：
./tools/testing/Invoke-TestPlan.ps1 -Profile Host
./tools/testing/Invoke-TestPlan.ps1 -Profile Host -Mode asan
# 本机现有 GCC / Keil 环境：
./tools/testing/Invoke-TestPlan.ps1 -Profile PreFlash -Compiler gcc -Jobs 2
# 独立定位和非放行诊断：
./tools/testing/Invoke-TestSuite.ps1 -Suite speed -Compiler gcc
./tools/testing/Invoke-TestPlan.ps1 -Profile Diagnostic -Suite speed,field-protocol -Compiler gcc
./tools/testing/Invoke-TestPlan.ps1 -Suite motion -Variant diagnostic -Compiler gcc
```

必要时使用 `-VcVars`、`-UV4`、`-ArmBin` 或对应的 `SMARTCAR_*` 环境变量指定
工具链。当前主机缺少 MSVC/可用 GCC ASan 库，环境阻塞不应伪装成业务断言失败。
不要求重现时改用个人用户名路径或关闭已有安全断言。

每轮生成 UTC Run ID，JSON 是结果唯一来源，Markdown 可从 JSON 重建。
`.test-results/` 与 `.build/` 不提交；CI artifact 有保留期限，Git 中保存运行链接、
摘要和来源身份，不承诺这些链接永久可下载。大文件、原始日志、私有机器快照
及 C31 patch/diff dump 继续本地保留，不为了 C32 归档一并上传。

## 9. 文档与证据自查

本记录使用原生 Markdown/源码、JSON、工程 XML 与 Git 身份。结构化记录为
`structured_parsed`，执行结果、哈希/正文对照为 `computed`。没有仅凭
`md_extracted` 确认的公式、单位或测试数值，也没有 `pdf_verified`/硬件测量声明。

提交前自查包括：提交范围白名单、生产目录差异、既有工作文件哈希、Schema/
DAG、公共夹具正文、链接目标、计数与原报告一致性、暂存执行代码与已通过 CI
快照的 Git blob 对照。源码工程归档、自动软件验证、正式烧录放行和实车验收
在本文及当前状态入口中分别表述，没有互相替代。
