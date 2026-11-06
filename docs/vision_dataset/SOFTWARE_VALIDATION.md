# PC 数据链条软件验证（2026-11-04）

基准：`main` / `de1415743b004160f520d385c9e3e63008f52007`，工作区 dirty。
本轮用户授权 PC 软件实现及 synthetic/mock 验证；没有真实硬件或 Label Studio 写入。
工具入口和使用流程见 [README](README.md)。本页记录本轮实际执行，不借用历史通过数。

## 已执行

| 检查 | 本轮结果 | 边界 |
| --- | --- | --- |
| `python -m unittest discover -s tests/vision_data -p test_pipeline.py -v` | **33 tests，全部通过**，最终约 12.34s | 标准库临时目录、synthetic 帧、mock transport；包含实际生产视觉 Host 编译和执行 |
| 最终测试输入哈希 | 测试前后及演示后保持一致 | 新工具/测试 Python、C、JSON、XML；演示另外记录生产头文件、视觉和运行时源码哈希 |
| 原生 JSON Schema | PowerShell `Test-Json` 校验 10 类实体通过 | 9 类由演示产生，另有一个指向真实 synthetic Frame ID 的 Event 样例；不是完整真实数据集验收 |
| 模板 XML | `label_config.xml` 解析通过 | 不是现有 Label Studio 实例的 UI/API 兼容性验收 |
| C32 清单 | `pwsh -File tools/testing/Test-Manifest.ps1` 通过，**66 suites** | 原 Runner、清单、CI 未改；本轮未重新运行原 Host/PreFlash 全套 |
| 生产/框架差异 | `git diff --exit-code -- demo/code libraries tools/testing tests/framework tests/suites/catalog.json .github` 退出 0 | 没有生产控制/视觉算法或全局测试基础设施修改 |
| 最终端到端演示 | **SYNTHETIC_SOFTWARE_CHAIN_PASS**，退出 0 | 2 Session / 10 帧、6 Regression + 4 Holdout；真人审核和真实赛道会话均为 0 |
| C31 Host 工具链 | GCC MinGW x86_64 14.2.0，C11/O2、实际宏和命令记录 | 两种运行模式各实际编译生产 `vision_midterm.c`；不链接车辆执行器 |

最终演示命令：

```powershell
python tools/vision_data.py demo --output C:/shengchanli/smartCar/vision-data-acceptance-20261104
```

输出独立于 Git：`C:/shengchanli/smartCar/vision-data-acceptance-20261104/`。
本轮曾生成两个较早的 synthetic 演示目录，均保留，未覆盖用户目录；本页引用上列最终目录。

该目录中的 `demo-report.json` 是流程索引。`dataset/runs/demo-frame/` 与
`dataset/runs/demo-sequence/` 分别保存 run.json、summary.md、原始 JSONL、输入流、
编译命令/日志、源文件哈希、可执行文件哈希和独立 Predictions。
两个运行使用同一 6 帧 Regression Release，均为 warmup=1、repetitions=3，
每种模式实际记录 18 个测量样本；warmup 不进入统计。Holdout 独立发布，未混入训练。

最终 Regression Release 的 SHA256：
`4cd6f7b402a822714996e36856554562ab1cd12b54f0333a77426b06d3a779d5`。
这是本轮 synthetic 内容身份，不是批准的真实数据集版本或性能基线。
未提供真实批准基线的比较明确输出 **INCONCLUSIVE**；没有自动提升 baseline。

## 所覆盖的重要故障与隔离行为

- CRC 标准向量、字节布局、分段连续性、文件头身份及显式 draft opt-in。
- 记录损坏后的扫描恢复、最后帧截断、缺 END、重复/倒序/缺口、非单调时戳及计数不一致。
- 源字节保留、stride padding 与可见像素区别、逐像素 PNG 一致性、被篡改图像拒绝。
- 路径越界、Git 内数据根、重复 JSON 键、非有限数、非法几何/Mask 类别拒绝。
- 全序列保留、可复现抽样、事件上下文、跨分区 exact pixels 泄漏拒绝和近重复提示。
- 标注导入不批准、重复导入幂等、原生 Canonical 降级、不允许 sidecar 偷换 Frame ID。
- 明确 reviewer/人工声明和审核状态转换；缺批准不生成假真值；Release/审核哈希链检查。
- Holdout 隔离、C31 自评污染拒绝、候选曝光不能由人工导出清除。
- mock 分页、dry-run、冲突、超时已创建后的查证、重复批次不重复写入。
- 仅服务选定图像的 loopback 素材服务；实际短时测试 GET、未选图像/越界/目录拒绝和 POST 拒绝。
- 真正的 C31 单帧/序列执行、重复输出确定性、Prediction 独立存储。
- 即使候选运行更快，质量退化仍使比较 FAIL，缺批准基线不能 PASS。

开发中首先发现 Windows 不能删除仍打开的 reservation 文件，已改为先关闭再删除；
另发现 percent 坐标往返的浮点尾差，改为仅对未变更投影恢复精确 sidecar 坐标，
真实修改仍采用新坐标。以上均由最终通过的测试覆盖，没有放宽安全或质量断言。

## 证据文件

本地原始记录在 `.build/vision-data-validation/`，不提交生成产物：

- `unittest-final.log`：最终 33 项逐项结果。
- `test-execution.json`：退出码和测试输入哈希。
- `schema-validation.json`：独立原生 Schema 校验的实体类型。
- `validation-summary.json`：本轮 Git、编译器、演示/Release/run 哈希、主计划 SHA256 和范围摘要。

规范、源代码、Schema、小型测试代码进 Git；原始帧、演示图片、可执行文件、完整运行
产物位于仓库外或忽略目录。当前任务没有 commit/push；此前用户未提交资料均保留。

## 未关闭事项

1. **正式 Recorder 格式未冻结**：当前仅 SCRAW-DRAFT-1；不能宣称已兼容真实车载导出。
2. **真实采集尚未实现**：无固件 Recorder、SD 写入、Sensor/Capture FPS、DMA 所有权、
   Cache 或控制实时性硬件证据。C31 的 R1/R2/R3 风险不因 PC 验证关闭。
3. **真实 Label Studio 未试点**：版本、端口、权限、导出 metadata 保留能力仍未知。
   当前是文件接口/原生 Canonical 降级和 mock 批次；无真实 HTTP 项目写入 adapter。
4. **无真实 Ground Truth**：演示所有审核 actor 为模拟；真实人工提交、独立审核、
   环岛/十字困难场景和泄漏人工复核仍需用户数据试点。
5. **评测覆盖有限**：已实现几何/覆盖率、预处理 Mask IoU、序列步差和 Host 成本；
   T1 十类分类及事件漏检/阶段指标尚无生产输出映射与独立审核 Event adapter，
   报告明确 NOT_MEASURED。完整车辆控制、Bayes 调度、MCU WCET 不在消费者中。
6. **无真实性能批准基线**：只验证比较流程与独立门禁，不宣称算法准确率或速度提升。
7. **下游注册未做**：现有 C32 框架保持原样；数据消费者可独立调用，未来按单独配置
   变更接入全局 Profile，不能把本轮 33 项说成原 PreFlash 已包含它们。
8. **规模与身份边界**：索引内存随帧数增长，近重复报告有限；哈希不是数字签名，
   reviewer 为操作者声明。未做长时/百万帧数据压力或真人身份认证。

## 阅读层次

主计划和有关上游任务书为原生 Markdown，直接读取；源码/JSON/XML 定点核验。
JSON/工程/配置/Schema 属于 `structured_parsed`；本页测试、哈希、CRC、像素和
原生 Schema 校验属于本轮 `computed`。没有仅 `md_extracted` 的精确结论，
没有 `pdf_verified` 或真实硬件证据。本轮未修改全局配置、Skill 或持久 Agent 行为。
