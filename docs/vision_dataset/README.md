# PC 视觉数据处理链条

更新：2026-11-07。入口：`python tools/vision_data.py --help`。Python 3.10+ 标准库；
C31 离线评测另需 GCC C11（当前使用 GCC），无需修改 Label Studio/Conda 环境。
PC主链已有synthetic/mock验证及SCRAW-1训练版adapter，源码已合入main。B2另有[真实静止短窗导出与PC重建](../changes/recorder-camera-stages-20261106/B2_CAMERA_SESSION_VALIDATION.md)，它不替代本主链的真人审核、Dataset Release、真实运动留出数据集或A/B评测。当前main软件验证见[主线同步记录](../changes/2026-11-07_MAIN_DOCUMENTATION_SYNC.md)。

接手入口：[2026-11-04 架构交接](ARCHITECTURE_HANDOFF_2026-11-04.md)，重点列出
已完成的模块和仍需完成的上下游架构、实施顺序与验收条件。

后续车载工作见 [采集训练版实施说明](../vision_recorder/IMPLEMENTATION.md)：独立训练
构型已完成最小RTOS、原始帧/SD持续写入与收尾软件；源码随main发布，稳定运动中的并行记录仍待验收。
SCRAW-1 可直接 `import`，旧 SCRAW-DRAFT-1 继续要求 `--allow-draft`；缺失/损坏最终
清单可从有效段头只读恢复，结果为 incomplete，源文件不改写。

```text
已导出的源会话 → CRC/序号/时戳验证 → A0 归档及清单锁
    → A1 可见像素 / A2 无损 PNG → 全序列 / 均匀和事件视图
    → 离线任务包 → 机器 Prediction（独立、可选、禁止 Holdout）
    → 人工文件提交 → Canonical QC → reviewed → approved
    → Session 级切分与泄漏检查 → Dataset Release + Lock
    → C31 单帧 / 序列 → 质量、覆盖率、Host 计算/进程耗时 → 独立比较门禁
```

审计：[固件基线](../vision_recorder/C31_BASELINE_AUDIT.md)、[VDS-0](VDS0_AUDIT_REPORT.md)、
[兼容性](UPSTREAM_DATA_COMPATIBILITY.md)。暂定字节协议见 [DATA_FORMAT](../vision_recorder/DATA_FORMAT.md)。
实际软件运行结果见 [SOFTWARE_VALIDATION](SOFTWARE_VALIDATION.md)。

## 一条命令验证整条软件链

在仓库根 PowerShell 执行，下列输出目录必须不存在且位于 Git 仓库外：

```powershell
python tools/vision_data.py demo --output C:/shengchanli/smartCar/vision-data-demo-new
```

演示生成 2 个明确 synthetic 的源会话、10 帧原始数据、Regression/Holdout 分区、
离线任务和模拟人工提交/审核、2 份 Release、C31 frame/sequence 运行和未获基线
批准的 INCONCLUSIVE 比较。**演示中的 reviewer 是模拟 actor，真实人工审核为零，
真实赛道会话为零。** 它不能用作真实准确率/性能或实车验收。

重复执行不能覆盖已有目录；使用新目录名。`demo-report.json` 是整链运行索引。
没有启动已有 Label Studio，没有烧录、串口或 SD 卡硬件操作。

## 常规使用

`$Data` 必须独立于 `smartcar` Git 仓库。`--source` 是已经导出到 PC 的目录，
工具不搜索磁盘、不读取块设备、不格式化或删除源数据。先核对源Session声明的格式：SCRAW-1按对应adapter导入；只有旧SCRAW-DRAFT-1示例才显式增加`--allow-draft`，该选项不授权解释未知格式，也不批准数据质量或硬件验收。

```powershell
$Data = 'C:/shengchanli/smartCar/vision-dataset'
python tools/vision_data.py import --source C:/local-export/session-001 --data-root $Data
python tools/vision_data.py verify-session --data-root $Data --session-id session-001
python tools/vision_data.py view --data-root $Data --view-id uniform-v1 --sessions session-001 --every 5
```

导入保留源文件，对复制前后和提交前的源哈希复核；失败保留 `.pending-*` 诊断目录，
不会将它发布为 Session。完整记录可恢复时生成 PNG 和逐帧 JSONL/CSV；异常恢复也
如实保留，但状态为 incomplete/invalid，命令返回非零，Release 默认拒绝。

`view --events events.json --context 3` 可加入事件窗口。Event 的最小记录为：

```json
{
  "event_id": "intersection-01", "session_id": "session-001", "event_type": "intersection",
  "start_frame_id": "session-001:120", "end_frame_id": "session-001:155",
  "phase_segments": [], "boundary_confidence": "uncertain", "review_status": "draft"
}
```

文件顶层为 Event 数组，起止和每个 phase 段必须引用该 Session 的真实 Frame ID；
phase 不得重叠或倒序。不确定边界保留 uncertain，不伪造精确起止。
Event 记录供上下文/索引用，不因一个 `review_status` 字符串自动成为评测真值。

### 划分与任务包

先人工准备 `assignments.json`，顶层为 Session ID 到角色的映射，例如：

```json
{"session-001": "regression", "session-002": "holdout", "session-003": "train"}
```

允许 train、validation、test、regression、performance、challenge、holdout；每个
Session 只能有一个角色。不同角色存在完全相同像素 SHA256 时拒绝生成。近重复
average-hash 候选和 layout 重叠是人工排查报告，不是零泄漏证明。先完成分区再产生
人工任务，避免已经看过预测的材料被事后标成盲化 Holdout。

```powershell
python tools/vision_data.py split --data-root $Data --split-id split-v1 --assignments C:/local-work/assignments.json
python tools/vision_data.py tasks --data-root $Data --view-id uniform-v1 --split-id split-v1 --output C:/local-work/tasks-v1
```

任务包只复制选中 PNG，带帧尺寸、像素/PNG 哈希、前后帧 ID、变换、split 和 exposure
信息。它不是整个 A0 数据根的文件服务。默认素材 URI 是相对路径，供离线交换。

若后续已审核本机 Label Studio，可以创建另一任务包时加
`--image-base http://127.0.0.1:9876`，手动导入 `tasks.json`，并由操作者显式启动：

```powershell
python tools/vision_data.py serve --exchange C:/local-work/tasks-http --port 9876 --origin http://127.0.0.1:8080
```

服务仅绑定 127.0.0.1，允许选定 PNG 的 GET/HEAD，禁止目录列表、任意路径、POST 和
素材变更；CORS 仅为给出的本机 Label Studio origin。该服务不会创建/修改 Label
Studio 项目；模板 `tools/vision_data/label_config.xml` 需在实际实例另行试点。
普通 import/evaluate/demo 不启动服务。

### 标注提交、候选和审核

T1 主场景、环境属性、T2 role/visibility/polyline/keypoint、T3 Mask/topology 是
Canonical 契约，定义在 `tools/vision_data/contracts.schema.json`；`validate --type
annotation --input <JSON>` 校验单个对象。CLI/库使用固定 Schema 子集验证器，未知
关键词拒绝，不声称实现任意 JSON Schema。跨文件、身份、几何、审核规则另由各模块检查。

Label Studio 文件导出转换：

```powershell
python tools/vision_data.py convert --data-root $Data --view-id uniform-v1 --exchange C:/local-work/tasks-v1 --export C:/local-work/export.json --instance local-ls --project 12
```

`instance/project/task/annotation` 构成外部身份，原始导出字节和哈希保留；
cancelled/draft 单列，空结果不自动代表背景，completed/approved 字段不授予批准。
只支持当前已实现的 Choices 和有明确元数据的有序 KeyPointLabels 投影；未知 brush
RLE/其他 result 类型拒绝，不静默丢失。开放折线不伪装为闭合 Polygon。若原 sidecar
的几何分组在 UI 往返后丢失，会要求显式 Canonical 修订，而不是猜测点序。
未变更投影的浮点误差 <=1e-10px 时恢复 sidecar 原坐标。

无需 Label Studio 的原生文件降级入口：

```powershell
python tools/vision_data.py import-canonical --data-root $Data --view-id uniform-v1 --exchange C:/local-work/tasks-v1 --input C:/local-work/submitted.json
```

`submitted.json` 是 Canonical Annotation 数组；必须 kind=annotation、origin=human、
status=submitted、独立 actor 和时间，保留工具/版本/曝光血缘。T2 空几何必须明确
geometry_state=`unannotated` 或 `explicit_empty`，T1 不伪造几何。T3 Mask 是相对输入
文件的 PNG 引用 `{path,sha256,class_map_version:"track-1"}`，必须为原图尺寸 GRAY8，
像素仅 0 背景 / 1 赛道 / 2 内岛 / 255 Ignore。没有 Mask 则为 null/缺省，不生成假背景。
Topology 仅 image_observation，不混入路线意图；sidecar 不能直接声明 approved。

上述两条转换命令返回 bundle_id；在其 canonical.json 中取得 Annotation ID。
实际操作者逐项检查后，先 reviewed 再 approved：

```powershell
python tools/vision_data.py review --data-root $Data --bundle-id <bundle> --annotation-id <annotation> --action reviewed --reviewer <reviewer> --notes '<具体审核依据>' --human-attestation
python tools/vision_data.py review --data-root $Data --bundle-id <bundle> --annotation-id <annotation> --action approved --reviewer <reviewer> --notes '<批准依据>' --human-attestation
```

Reviewer 必须不同于 annotator。记录只追加版本和哈希链，不修改人工提交；拒绝后
应新提交修订。真人身份由操作者声明，工具没有身份认证/签名能力，不能据此声称机器
已经核实真人。本轮 demo/test 只使用 synthetic 模拟 reviewer。

机器输出在 evaluate 的 `predictions.json`，kind=prediction、status=draft，独立于
人工 Annotation。`prediction-exchange --run-dir <run> --exchange <tasks> --output
<new-tasks>` 将候选写到新的任务包，绑定输入哈希和模型/Git，并持久记录曝光。
Holdout 拒绝 Prediction；原生/LS 导入都不能通过删除外部字段擦除锁定任务包的曝光。
评价 C31 时有 C31 曝光或不明 Prediction 血缘的标注拒绝使用，需要独立重标。

### Release 与 C31 离线评测

```powershell
python tools/vision_data.py release --data-root $Data --release-id regression-v1 --view-id uniform-v1 --split-id split-v1 --bundles <bundle> --purpose regression
python tools/vision_data.py verify-release --data-root $Data --release-id regression-v1
python tools/vision_data.py evaluate --data-root $Data --release-id regression-v1 --run-id frame-v1 --mode frame --warmup 1 --repetitions 3
python tools/vision_data.py evaluate --data-root $Data --release-id regression-v1 --run-id sequence-v1 --mode sequence --warmup 1 --repetitions 3
```

Release 只选 purpose 对应的 Session，冻结来源、采样、split、人工提交和审核快照，
记录所有未标注帧。重复 Release ID 拒绝；同帧多份 approved 需先裁决。没有真值也可
做无真值的观测运行，但不生成假准确率。校验在评测前后都进行，源码/数据中途变化失败。

Adapter 直接编译当前生产 `vision_midterm.c`；**不链接执行器，不替代原 Equivalence**。
Frame 模式每帧 reset；Sequence 模式按真实顺序推进，跨 Session、源序号缺口、非单调
或超过当前 HISTORY_GAP_MS 的间隔 reset。不能把抽样序列的状态延续说成完整连续采集。

输出分开记录：逐帧几何 MAE/RMSE、标注可见行的检测覆盖率、可用 Mask 上的预处理
track IoU；有效连续帧的 error_px 步差；Host 算法 compute_ns 与含 IO 的进程 wall_ns。
重复运行检验输出确定性，warmup 不计入统计；统计含均值/中位/P95/P99。序列步差
不是车辆转向抖动，Host 耗时不是 MCU 周期/WCET。分类 taxonomy 与事件阶段评测适配
尚未实现，报告 NOT_MEASURED；不从 roundabout_state 硬拼十类 T1 真值。

`compare --baseline <run.json> --candidate <run.json> --approval <approval.json> --output
<new-result.json>` 比较固定 Release、参数、编译器、环境、帧和重复配置。批准文件必须
绑定基线 run SHA256、clean Git SHA、reviewer、approved_at，不自动晋升最新 main。
质量、覆盖率、Mask 与速度门禁独立，快不能抵消质量退化；噪声区间或未批准基线为
INCONCLUSIVE，返回非零。该比较属于新数据消费者，不改 C32 benchmark.py 的旧契约。

## 存储和实现边界

```text
<data-root>/
  sessions/<sid>/A0/                 原字节及源 JSON
  sessions/<sid>/A2/                 无损 PNG
  sessions/<sid>/frames.jsonl        A0 偏移、帧号、时戳、哈希、A1 对应
  sessions/<sid>/frame_manifest.csv
  sessions/<sid>/integrity.json
  sessions/<sid>/session_manifest.json + archive.lock.json
  views/<id>/view.json + view.lock.json
  annotations/<hash>/canonical.json + original-export.json + masks/ + bundle.lock.json
  reviews/<bundle>/000001.json ...   追加的审核版本/哈希链
  splits/<id>/split.json + split.lock.json
  releases/<id>/release.json + dataset.lock.json
  runs/<id>/run.json + run.lock.json + predictions.json + summary.md + 原始输出/编译日志
```

工具保证不覆盖已有 ID，消费时重新核对哈希；这不是文件系统 ACL 或密码学签名，
不能防止拥有目录写权限的人同时重写内容和全部锁。进程崩溃保留的 reserve/半成品
需先诊断，不自动删除/覆盖。解析只持有一帧 payload，索引/抽样/JSON 清单内存随帧数
增长；不是已证明的无限规模流式数据库。近重复候选采用分带索引和代表项，最多报告
1000 对；超大数据的性能还需专项验证。

Connector 的默认 dry-run 和 mock 注入验证幂等、冲突、分页、超时后查证及继续执行。
公开 CLI 没有真实 HTTP 项目写能力，也不接受 Token。真实 API adapter/实例能力试点
后再接入，文件流程在此之前可独立使用。不修改原有 Conda、生产固件、测试 Runner/CI。
