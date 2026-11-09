> **维护口径（2026-11-09）：** `00bb7a4`、72定义、49 PASS/1 SKIPPED等数字属于2026-11-06被测版本，不能代表最新HEAD或未提交catalog。Phase2整合提交bd94f27另有41项Python与19350项模拟IO断言，本轮未核定该提交远程CI；它们不是统一Host/PreFlash套件计数，也不代替实车验收。 统一状态与历史适用范围见[文档维护总览](DOCUMENTATION_STATUS.md)。

# SmartCar 测试体系

维护：2026-11-07；原始C32架构交付：2026-11-02。统一入口位于 `tools/testing/`，套件定义位于
`tests/suites/catalog.json`。普通执行只做 Host 模拟、离线协议/工具测试、
静态检查和目标纯构建，不烧录，不打开串口，不连接车辆或执行器。

完整使用、开发、维护和排障流程见 [测试框架与流程详细手册](TESTING_FRAMEWORK_GUIDE.md)。
本页保留架构概要，精确运行事实仍以验收记录为准。

2026-11-06清单72个定义，Host/PreFlash/Diagnostic依赖闭包分别50/64/48个；Recorder与规划扩展已合入main。合并提交`00bb7a4`的远端optimized/ASan各49 PASS / 1 SKIPPED，缺数据的performance-host保留接口。[当前验收](testing/VALIDATION.md) · [主线同步记录](changes/2026-11-07_MAIN_DOCUMENTATION_SYNC.md)

## 使用

需要 Windows、PowerShell 7、Python 3.10+ 和 Git。MSVC 是默认 Host 工具链；
安装支持 C11 的 C++ Build Tools，ASan 另需对应组件。框架本身只用 Python
标准库与 PowerShell `Test-Json`，不需要 pip 安装。请从 PowerShell 执行：

```powershell
./tools/testing/Invoke-TestPlan.ps1 -List
./tools/testing/Test-Manifest.ps1
./tools/testing/Invoke-TestSuite.ps1 -Suite Speed
./tools/testing/Invoke-TestPlan.ps1 -Profile Host
./tools/testing/Invoke-TestPlan.ps1 -Profile Host -Mode asan
./tools/testing/Invoke-TestPlan.ps1 -Profile PreFlash
./tools/testing/Invoke-TestPlan.ps1 -Profile Diagnostic -Suite speed,field-protocol
./tools/testing/Invoke-TestPlan.ps1 -Suite motion -Variant diagnostic
```

未找到 MSVC 时可传 `-VcVars <vcvars64.bat>`，或设置 `SMARTCAR_VCVARS`。
发现顺序为现有 `cl.exe`、显式配置、vswhere、常规 VS 安装目录。编译器必须
通过实际编译及运行探针才能开始依赖它的测试。

可显式传 `-Compiler gcc` 进行补充 Host 验证。它不会伪装成 MSVC /W4 /WX 或
MSVC ASan 的结果。GCC 使用 C11、优化和警告即错误；仅关闭现有紧凑排版及
未使用静态辅助函数的警告。编译参数和版本会完整记录。

Keil 可用 `-UV4` 或 `SMARTCAR_UV4` 指定，ARMCLANG 可用 `-ArmBin` 或
`SMARTCAR_ARM_BIN` 指定。框架也探测现有脚本使用过的标准路径。PreFlash
保留完整 Host 范围，并添加工程/静态检查、ARM 源码编译及 base/train/route/
switch/auto-track/link/threshold/motion/speed 九个纯构建。不会调用 flash 脚本。

缺少 MSVC 只阻塞相关 C 套件；独立的离线 Python/协议测试仍可执行。缺少
Keil/ARM 工具会阻塞相应目标证据，不会将环境缺失报告成业务断言失败。

## 结构和边界

| 组件 | 实现 | 职责 |
| --- | --- | --- |
| 用例/公共夹具 | `tests/vision/`、`tests/support/fixtures/` | 原有场景、断言、参考算法、模拟 IO |
| Manifest/契约 | `tests/suites/catalog.json`、`tests/schemas/` | 身份、分层、宏、源码、DAG、资源、证据类型 |
| Runner | `process_runner.py`、`providers.py` | 工具准备、独立进程、超时清理、原始日志 |
| Orchestrator | `Invoke-TestPlan.ps1`、`run_tests.py` | Profile、依赖闭包、串行或有限并行、失败传播 |
| Reporter | `reporter.py`、`Write-TestReport.ps1` | 从持久化 JSON 生成 Markdown |

PowerShell 是公开 Windows 入口，Python 标准库负责进程/DAG/报告的共同执行内核，
避免用 PowerShell 字符串拼接实现复杂进程树和图调度。原有 Python 离线测试
也继续使用同一个 Python 解释器。套件不导入其他套件的 main，不依赖执行顺序
产生的进程内状态；引用生产模块的 C 文件继续通过显式链接参与测试。

清单经过真正的 draft-7 JSON Schema 校验，以及唯一 ID、唯一 Variant、入口后缀、
路径存在/范围、依赖存在、DAG 无环和硬件边界校验。路径解析包含符号链接后的
最终路径检查；不允许 `..`、绝对路径和仓库外入口。

`workingDirectory` 是套件私有执行沙箱内的相对目录（当前均为 `.`），不是源码
共享目录。实际 CWD 会逐进程记录。源码/脚本路径始终相对于仓库根，C 源文件、
包含目录、宏和参考对象均显式列出。所需只读 fixture 会复制到对应 Variant。

L0–L5 与任务书一致：Environment、Unit、Integration、Contract、Performance &
Quality、Build & Static、Hardware Acceptance。配置契约仍属于 L3-A，执行方式
另记为 `host_compile`。Host、Static、Target Compile、Target Build、PQ 和 Hardware
证据分开，不以主机通过推断板端行为。

## 独立执行和兼容迁移

现有十个 `tests/vision/run*.cmd` 均保留，改成统一入口的薄包装。首参数 `asan`
仍选择 ASan，`run_motion_tests.cmd optimized diag` 仍只选择 diagnostic Variant。
默认仍用 MSVC；为验证兼容包装可临时设置 `SMARTCAR_TEST_COMPILER=gcc`。
旧脚本分组对应清单中的 aggregate 节点，aggregate 只归并显式依赖，不执行测试。

共享的速度 IO、速度辅助函数、车道、开关和运行时模拟模型已抽到五个受保护
头文件。保留所有原有场景主体及安全断言。`test_midterm_boot.c` 的重复速度 IO
也复用公共模型。`tests/vision/stubs/` 和历史算法 reference 保留原址；没有为了
目录美观搬迁不相关代码。参考算法单独编译并重命名符号，不进入 MCU 工程。

`vision-contract` 验证源码中已有的分层承诺：全局统计、灰度预览、均匀场景的
历史参考一致性，以及行 Otsu 的独立排序/有理数 Oracle。不把已经有意改变的
跟踪/叠线行为列入等价承诺。94×60 实拍静态片段继续是功能/契约 fixture，
不转化为全分辨率标签或真实控制时序。

新增配置矩阵编译真实生产头文件。非法组合必须同时得到非零编译退出码和
对应生产 `#error` 诊断；缺头文件、编译器崩溃、其他编译错误不能算通过。
新增接口契约只断言公开字段宽度和几何关系，不绑定编译器结构体填充。

## 状态和执行策略

| 状态 | 含义 |
| --- | --- |
| PASS | 套件实际完成且通过 |
| FAIL | 测试/协议断言、期望配置拒绝或明确构建要求失败 |
| ERROR | 编译、工具或执行基础设施错误 |
| TIMEOUT | 总执行期限耗尽，包含等待资源及进程执行 |
| BLOCKED | 必要环境或依赖未通过，未执行相关业务测试 |
| SKIPPED | 当前无需执行，例如未接入数据集的可选基准 |
| MANUAL_PENDING | 只能由独立授权与真实测量补充的人工证据 |

同层独立失败继续收集，下游依赖失败标记 BLOCKED。必需项中任何非 PASS 状态
都会让整体无法通过。可选基准的 SKIPPED 不影响其他模块；报告仍列出未测量
范围。Diagnostic 不作为放行依据；单套件结果也不冒充完整 Profile。

默认 `-Jobs 1` 串行。输出和夹具隔离验证后可显式用 `-Jobs 2`，最多 4。
依赖完成后才调度下游；Keil、ARM 工具及仍共享内部离线工具暂存位置的套件
使用跨运行的 OS 文件锁。Keil 始终串行。锁由 OS 在进程退出时释放，残留
`.lock` 文件不代表资源仍被占用。

Windows 子进程先挂起创建、加入 Kill-on-close Job Object，再恢复运行；避免
子进程在归组前逃出。超时会终止整个 Job，等待已捕获进程句柄并检查活跃进程
归零。成功退出时也清理未回收后代，逐进程保留 `cleanupVerified`。框架自测
实际创建父/子进程和持续写入探针验证清理，不只模拟返回码。

所有 Host EXE、OBJ、PDB、生成图像、日志在本轮/套件/Variant 下。目标构建复制
`demo/` 和 `libraries/` 到套件私有工作区，并重定向 Objects/Listings；不会覆盖
已有未提交的竞争工程文件或生产发布目录。ARM 源码检查也在私有副本运行。

## 证据与可复现性

`.test-results/<UTC-run-id>/` 包含：

```text
run.json                 # 结构化结果唯一来源
summary.md               # 从 run.json 生成
environment.json         # 平台、工具版本、输入文件 SHA-256
suites/<id>/result.json
suites/<id>/stdout.log
suites/<id>/stderr.log
suites/<id>/artifacts/    # Variant 产物、fixture、副本工程及构建日志
```

记录 Git SHA、branch、dirty、工作区 diff 哈希、清单哈希、全部相关源码/夹具/
执行器文件哈希、工具版本和进程参数、宏组合、Variant、耗时、退出码及清理证据。
运行前后输入哈希不一致时整体为 ERROR，不将混合状态执行作为验收。

`Write-TestReport.ps1 -RunJson <path>` 可重新生成报告，不重新解释 stdout 猜测
断言结果。suite JSON 保存实际命令，原始 stdout/stderr 同时保留。生成目录被
Git 忽略，CI 将其作为 artifact 归档。

显式 `-Suite` 选择的目标在本轮按必需项处理，因此直接选择硬件占位只会得到
MANUAL_PENDING，不会因为环境通过而汇总成 PASS。套件标准退出码为 PASS/
SKIPPED=0、FAIL=1、ERROR=2、BLOCKED=3、MANUAL_PENDING=4、TIMEOUT=124；
原始子进程退出码另行保存。负向编译通过时套件退出码为 0，编译器拒绝的
原始非零码仍可查证。计划退出码为通过 0、非通过 1、执行前框架错误 2。

GCC 补充验证、单套件和 Diagnostic 均不会给出 `releaseEligible=true`。该字段
即使在 MSVC 完整 Profile 通过时，也只限所记录的软件范围，不等同于烧录或
硬件运行授权。MSVC ASan 构建额外串行保护 PDB 服务，避免并行进程互相清理。

## 性能与质量

接入规范见 `tests/benchmarks/README.md`。`performance-host` 当前为可选 pending
接口，输出 SKIPPED；已有合成控制/视觉场景继续执行。数据集、标签、采样 adapter
和批准基线接入之前，不声明真实准确率、性能提升、MCU WCET 或系统时延通过。

已实现版本/哈希契约、原始样本导入、有效样本/重复次数校验、均值/中位数/分位数、
独立质量/计算/系统指标、绝对/退化/不退化/观察门禁和噪声区间 INCONCLUSIVE。
批准基线必须有 reviewer/time/clean SHA，且比较条件一致；不会自动晋升最新 main。
性能比较命令无法判定时返回非零，但 pending 接口不会进入功能套件的依赖链。

## CI 与人工证据

`host-tests.yml`在Windows runner上调用同一清单和入口，分别执行MSVC optimized与ASan。原合并CI基准`00bb7a4`的两模式各49 PASS / 1 SKIPPED；原C32的43 PASS / 1 SKIPPED按其历史SHA保留。运行链接、精确提交和范围见[验收入口](testing/VALIDATION.md)。

`preflash-tests.yml` 仅人工触发，使用有正确设备包/许可的 `smartcar-keil` 专用
Windows runner。本次文档维护没有执行该远程任务，本地主机的ARM/Keil结果也不算远程CI。C32原始交付与当前MPC合并分别按各自SHA归档，见[主线同步记录](changes/2026-11-07_MAIN_DOCUMENTATION_SYNC.md)；代码归档不签发烧录放行。

自动静态检查覆盖工程引用、遗留入口约束、PWM 写入归属、fixture 内容及测试间
源码包含。完整 ISR/DMA 所有权、临界区正确性仍需要人工审查。相机、DMA、编码器、
实际 PWM/LDOK、中断/闭环时序和实车赛道行为始终属于独立人工/硬件证据。

## 本轮盘点和阅读层级

原始资产及哈希见 `testing/inventory-before.json`。任务书直接读取原始 Markdown；
当前环境没有 MarkItDown MCP，未进行二次文本转换。该源本身没有需要版面复核的
公式/表格图片。本轮没有只凭 `md_extracted` 作出的精确数值结论。

清单、JSON Schema、工程 XML 和 Git 状态属于 `structured_parsed`；执行与
哈希/前后对照属于 `computed`。没有 `pdf_verified` 或硬件实测证据。本轮具体
结果和未执行项见 `testing/VALIDATION.md`，历史日志不计入本轮通过数。
