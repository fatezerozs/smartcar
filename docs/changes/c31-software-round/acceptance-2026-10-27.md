# C31 烧录前软件验收交接（2026-10-27）

2026-10-27 历史结论：首要 PWM headroom 断言及 switch suite 已通过；软件验收尚未全部通过，剩余明确阻塞是预处理等价测试第 4 帧不一致。没有烧录、串口/硬件访问或实车操作。

## 工作区与边界

- 工作目录仅 `C:\shengchanli\smartCar\smartcar`；保留所有既有修改；未执行 commit/reset/clean/checkout。
- 初始 `git status --short --branch`、`git diff --stat`、`git diff --cached --stat`、`git diff --check` 均退出 0；暂存区为空。
- 已读取 `architecture-status.md`。Bridge 检查在线，报告当前项目 1 个活动任务，与本轮运行相符；线程无其他子代理，没有启动并行代理。
- 未读取凭据、修改 Bridge/Codex 配置、建立启动持久化、访问 `.build/video-deps` 或全仓递归扫描。
- 仅执行主机桩测试、定点源码检查和 Keil 纯构建。现有 PWM 精度、模式事件消费、Midterm Boot 夹具、确定性时钟修改未重做。

## 首要失败的证据与修复归属

用户交接的旧结果为 right=999 / left=994。当前工作区的 `wheel_speed.c` 已含积分跨越饱和边界时接受部分增量的修复：旧条件积分会拒绝整个越界增量，可能留下永久的次饱和间隙。这是生产 PI 饱和处理问题，不能靠放宽断言解决。

本轮首次实际运行中，原 `>=995` 断言已经通过：最后 5 个样本双轮均为 1000，目标 122/123、测量 1/1、kp_q8=256、ki_q8=12、faults=0。保留该既有修复，未再修改 PI、增益、安全阈值、努力上限或 PWM 映射。

检查了 `vision_speed.c` 的样本消费与 PI 调用、`vision_power.h` 的努力爬升上限和 `vision_actuator.c` 的 PWM 换算。该夹具每步推进 10ms 和编码器计数、每 20ms 发布视觉帧；没有靠额外推进时钟或延长 1500 步来通过断言。

随后 suite 在局部道路恢复检查失败：一次零需求无效帧使 `output_allowed()` 拒绝输出并调用 `vd_output_zero()` 撤销授权；控制器之后仍是 RUNNING，恢复需求增加，但授权为 0、目标和 PWM 一直为 0。

本轮最小生产修改：`vision_control.c` 将已有的最多两帧、40ms 零需求宽限状态标为 `recovering=1`。宽限期间 duty=0；既有授权仍有效，当前道路证据恢复后可继续受限恢复。没有更改失效帧数、40/300/1200ms 边界、故障或 STOP 撤销逻辑。C31 autonomous 仍由自身无效观测处理立即故障，auto-track 两种 review 测试均通过。

`test_turn_route.c` 增加零需求期间双轮为零且授权存在的检查；随后以慢速非零计数确认实际双轮 PWM 恢复。该低速模拟输入使正向扭矩可观察：原默认测量 15 ticks 高于恢复目标时，PI 合法输出零，不能断言必须有扭矩。原 headroom 断言及 1500 步保持不变；所有 HEADROOM/PARTIAL 临时诊断已删除，局部恢复等待恢复为原有 20 步。

## 已运行命令与真实退出码

下表 `.cmd` 均从项目根通过 `cmd /c tests\vision\<入口>` 执行；协议入口通过 `powershell -NoProfile -File tests/vision/test_field_protocol.ps1` 执行。

| 入口 | 退出码 | 结果 |
| --- | --- | --- |
| `run_switch_tests.cmd` 首次 | 1 | headroom 已通过；局部恢复目标为 0 失败 |
| `run_switch_tests.cmd` 第二次 | 1 | 原失败通过；新增正 PWM 检查暴露模拟测量过快的问题 |
| `run_switch_tests.cmd` 最终 | 0 | 全套通过；turn-route 371490 checks；map entry 801869 checks；preview 40342 checks |
| `run_speed_tests.cmd` | 0 | SPEED 13284 checks，encoder board 通过 |
| `run_link_tests.cmd` | 0 | link / transport / dual runtime 通过 |
| `run_equivalence.cmd` | 1 | `FAIL: preprocessing mismatch at frame 4` |
| `test_field_protocol.ps1` | 0 | 50 checks；无 COM 打开 |
| `run_auto_track_tests.cmd` | 0 | review=0: 29198 checks；review=1: 41396 checks |
| `run_tests.cmd` | 0 | 通用 Host suite 全通过，含 BOARD+MODE 248 checks |
| `run_midterm_tests.cmd` | 0 | Boot 各准入组合及 runtime 通过 |
| `run_route_tests.cmd` | 0 | 36605 checks |
| `run_motion_tests.cmd` | 0 | 默认诊断关闭组合，6346 checks |
| `run_motor_threshold_tests.cmd` | 0 | diag=0 / diag=1 均通过 |

后五项是最终生产修改后的补充相关回归。默认 motion 之外的专用 `diag` 参数组合和 ASan 未运行。

除 switch 的直接终端输出外，各入口日志保存于 `.build/c31-acceptance/`。未将任何失败包装成通过。

## Keil 与宏组合

命令：`powershell -NoProfile -File tools/build_midterm.ps1 -Variant <变体>`。

| 变体（依实际顺序） | 退出码 | 编译结果 |
| --- | --- | --- |
| auto-track | 0 | 0 Error(s), 0 Warning(s) |
| train | 0 | 0 Error(s), 0 Warning(s) |
| route | 0 | 0 Error(s), 0 Warning(s) |
| switch | 0 | 0 Error(s), 0 Warning(s) |
| auto | 0 | 弃用警告后映射 auto-track，0 Error(s), 0 Warning(s) |

脚本调用 UV4 参数为 `-r <project> -t nor_sdram_zf_dtcm -j0 -o <log>`，没有下载参数。核对 base 及生成项目的构建前后 RunUserProg1/2 均为 0；没有调用下载、调试或硬件入口。脚本还检查新鲜 AXF 存在；产物和日志均在 `.build/midterm-<variant>/`。工程中的 Flash 配置存在，不代表本轮调用了它。

生成 XML 的结构检查及源码定点引用审计确认：

- train：training=1、encoder=1、autonomous=0。
- route：train 基础加 route_trial=1。
- switch：route 基础加 route_switch=1，autonomous=0。
- auto-track / auto 别名：training=1、encoder=1、autonomous=1、route_trial=1、route_switch=1、auto_track=1、start_switch=0、visual_reviewed=0。
- C31 autonomous 派生模式仍由 autonomous && route_switch && route_trial 决定；auto_track 标记保留编译保护。
- 启动和开关宏仍有消费者，本轮没有删除宏；完整定点引用结果在 `.build/c31-acceptance/macro-references.log`（117 行）。
- `demo/code` 的实际 `pwm_set_duty` 调用仅位于 `vision_actuator.c` 的统一零输出与缓存提交入口。该静态检查不能证明硬件 ISR/LDOK 时序。

## 本轮文件变化

- `demo/code/vision_control.c`：零需求宽限的恢复状态修复。
- `tests/vision/test_turn_route.c`：移除既有临时诊断，保留 headroom 断言，补充零输出/恢复扭矩检查。
- `docs/changes/c31-software-round/acceptance-2026-10-27.md`：本交接。
- `docs/changes/c31-software-round/architecture-status.md`：增加本交接链接，保留原历史说明。
- 构建脚本生成 `demo/mdk/competition/midterm_auto-track.uvprojx`；重写了现有生成工程，switch 在 status 中有标记但 `git diff --` 无文本差异，train/route 无新增文本差异。没有手工改写工程策略。
- 测试/构建生成物位于既有 output 与 `.build`；所有起始未提交修改保留。

## 剩余阻塞与下一步

1. 优先审查预处理等价契约：`demo/code/vision_midterm.c:162` 使用逐行 Otsu；`tests/vision/reference/vision_reference.c:102` 起使用灰度范围映射和 deadband。`git diff --exit-code --` 对生产预处理、reference、test_equivalence 和入口四文件退出 0，说明此差异已在 HEAD 中存在，不能归因于本轮控制器修改。尚未独立证明第 4 帧具体像素差异的因果链，也未审核新算法 oracle；不要简单更新参考或删除断言来放行。
2. 完成等价契约/参考实现审查后，重新运行 equivalence 及受影响回归，才可关闭软件验收。
3. 既有全量改动的完整 diff 审查、全局 ISR 竞争证明、发布文件前后校验和对比尚未完成；本轮只做所述定点检查。未进行额外 ASan、motion diag 专用组合或无效宏组合负向编译审计。
4. 硬件、串口、实车及烧录均未执行，仍不在授权范围内；真实摄像头时序、编码器、PWM/LDOK 和停车行为不能用主机通过代替。

最终源码回归后 `git diff --check` 退出 0（仅既有 LF/CRLF 提示）；暂存区仍为空。自查：未改安全阈值或原断言，临时诊断已撤销，失败项保留，未把纯构建或模拟通过描述为硬件验收。

## 2026-10-28 续验：预处理分层契约

本节更新上文历史阻塞状态，保留原结果及架构历史。预处理阻塞已按分层契约解决，本节列出的主机检查通过；既有全量 diff 审查、ISR 竞争证明及发布文件校验和对比仍未完成，不能据此声称整个系统或硬件已验收。

### 诊断与契约判断

- HEAD 仍为 `78f2d534d3675c3679317e864ed1e51be8da2c66`。开始及结束均核对 status、HEAD、diff stat、cached stat、diff check；暂存区为空。`architecture-status.md` 从本记录同目录读取，其历史说明未修改。
- 临时诊断保持原种子及帧 0..15 的生成顺序。第 4 帧 binary 差异 **0/22560 像素**，row_threshold 差异 **87/120 行**，threshold 旧/新 **98/98**，adaptive_rows **33/87**，white_percent **62/62**，gray_low/high 均为 **3/149**。gray 和 binary preview 均为 **0/5640 像素差异**。
- ROI 为 y=24..110。第 4 帧 y=61..93 阴影带阈值旧/新 **32/25**，ROI 非阴影行 **98/79**，ROI 外均为 98。该帧失败来自阈值及计数契约变化，不能解释为道路二值图损坏。
- 第 7 帧随机纹理还产生 **423** 个 binary 像素差异、82 行阈值差异、**99** 个 binary preview 像素差异；gray preview 无差异，全图阈值均为 126。因此不能只跳过第 4 帧或只检查二值图是否兼容。
- `git log -S 'threshold = otsu(hist, VISION_WIDTH)'` 定位既有提交 `f82f67d`：该提交明确用逐行 Otsu 替换映射/deadband，同时新增 `test_vision.c` 的 bright-floor-object 回归。当前生产注释及头文件 adaptive_rows 说明与此一致。证据支持这是既有的有意算法修正，不是本轮生产回归；本轮没有改变生产算法。
- 原 harness 的“预处理全等价”契约已不准确。全图统计仍兼容；通过全图及行内对比度门槛的 ROI 行按本行分布选 Otsu，不再应用旧映射/deadband。阈值不同可以保持同一二值边界，也可能改变噪声分割。tracking/overlay 原本已排除，不能由此宣称整个视觉结果等价。

### 本轮改动与验收层次

只编辑 `tests/vision/test_equivalence.c` 与本记录。生产源码、旧 reference、运行入口、工程配置及 `architecture-status.md` 均未编辑。

1. 保留原有 6000 生成帧及全部 32896 个无序 8-bit 灰度对（含常量和对比度分支边界）。兼容场景由输入类型预先确定：生成帧无横向阴影、随机纹理或反色离群点；没有根据比较结果选择是否严格比较。
2. **36527 帧**继续逐像素严格比较 binary、所有行阈值、全图统计、adaptive_rows、white_percent 和 gray/binary preview；另 **2369 帧**执行明确的新局部语义断言，未无条件跳过任何帧。
3. **全部 38896 帧**还执行独立行 oracle：排序样本确定行内百分位门槛，用两类平方和/类大小最大化为目标，以 uint64 精确有理数交叉乘法比较，取最优阈值平台中点。没有复制生产浮点评分、直方图空桶优化或旧映射。188 个 8-bit 样本下交叉乘积小于 4e15，无 uint64 溢出。逐行验证实际阈值、逐像素三点投票、adaptive_rows、white_percent 及 preview 从各自输入缓冲的精确采样。所有场景的全图统计和 gray preview 仍与旧 reference 严格相等。
4. 另增解析阴影夹具：道路 x=34..153，原灰度 10/149，y=61..93 同比例除以 3。明确断言 ROI 非阴影/阴影阈值恰为 **79/25**、每行双边实测恰为 **34/153**、row_flags=3、结果有效。全高逐像素要求道路保持同一白色区间，验证阴影两处交界无断裂、扩边或漏边。
5. 隔离负向检查故意将源码副本的行阈值改为全图阈值，测试退出 **1**，报 `row oracle frame 4 y=24 expected=79 actual=98`。这证明新层会拒绝失去行适应的实现；真实生产源码未改。临时诊断/变异源码、命令及其专用产物均按明确路径删除。

### 真实命令与退出码

所有命令从项目根执行，仅主机运行。

| 命令 | 退出码 | 结果 |
| --- | --- | --- |
| `cmd /c tests\vision\diagnose_equivalence.cmd`（临时，已移除） | 0 | 定点量化前 16 帧，见上文 |
| `cmd /c tests\vision\run_equivalence.cmd`（重构后首次） | 0 | 38896 帧分层契约及解析阴影夹具通过 |
| `cmd /c tests\vision\run_auto_track_tests.cmd` | 0 | review=0: 29198 checks；review=1: 41396 checks；模拟 IO |
| `cmd /c tests\vision\check_equivalence_mutant.cmd`（隔离负向，已移除） | 1 | 预期拒绝变异实现，不计为生产验收通过项 |
| `cmd /c tests\vision\run_equivalence.cmd`（清理后的最终版本） | 0 | 同样 38896 帧及阴影夹具通过；日志 `.build/c31-acceptance/equivalence-round2.log` |
| `git diff --check`（最终） | 0 | 仅既有 LF/CRLF 提示，无空白错误 |
| `git diff --cached --quiet`（最终） | 0 | 暂存区为空 |
| 定点 Python 差异保留检查 | 0 | 排除本轮 test_equivalence.c 后，tracked diff 与起始快照逐字节相同；原 status 条目全部保留，新增 tracked 编辑仅该测试文件 |

本轮只改 equivalence harness，auto-track 是最小相关启动/锁止回归入口。本轮未重跑 route/switch 全套、其他已通过套件、ASan 或五种 Keil 构建：生产源码与配置没有本轮变化，前轮结果保留在上文，不冒充本轮执行。

### 剩余风险与自查

- 本轮没有新的生产验收失败。历史第 4 帧失败保留，其过时的全等价契约已按证据分层；负向变异退出 1 仅用于证明检测能力。
- 检查限于当前默认配置与几何。合成阴影及随机场景不代表真实相机分布；全图统计仍使用旧 reference 作兼容基线，未独立证明其所有语义；一般 tracking/overlay 正确性不在本次声明内。
- 最终 status/diff stat/cached stat、HEAD 及 diff check 已复核；既有 tracked 修改逐字节保留。验收记录原来位于未跟踪目录，其改动需单独阅读，不能只看 diff stat。临时文件已移除，没有意外源码或工程修改，未执行 commit/reset/clean/checkout。
- 始终只在批准项目内工作，未读取凭据、修改 Bridge/Codex 配置、建立持久化、访问 `.build/video-deps` 或递归扫描全仓。本轮未调用 Bridge、烧录、串口/硬件或实车入口。真实摄像头时序、编码器、PWM/LDOK 和停车行为仍未经硬件验证。

## 2026-10-28 最终静态审查

本次完成起始清单内 C31 相关 tracked **全部 38 个有文本差异文件**的逐文件 diff 审查，另核对 status 中无文本 diff 的 switch 工程和未跟踪 auto-track 工程。未修改生产源码、测试或工程；仅追加本节，未重跑已通过回归和五种 Keil 构建。没有发现本次能够独立确证并需要修改的生产逻辑 bug；但 **ISR/DMA 全局无竞争证明未闭合，不能把本次静态审查写成无条件烧录放行**。特别是 R1 的缓冲所有权、R2 的完成时间可信度、R3 的硬件时序仍有限制。旧 auto 目录产物确认过期。

#### 起始状态、归属与逐文件范围

- HEAD：`78f2d534d3675c3679317e864ed1e51be8da2c66`；分支 `main...origin/main`。起始七条指定 Git 命令按用户顺序执行，均真实退出 0：`git rev-parse HEAD`、`git status --short --branch`、`git diff --stat`、`git diff --cached --stat`、`git diff --name-only`、`git ls-files --others --exclude-standard`、`git diff --check`。起始统计 38 files / 837 insertions / 202 deletions，暂存区为空；28 个未跟踪文件。
- 表内“本轮已知”指此前 C31 软件轮及 10-28 续验，不代表本次新写。与 `preflight-existing.diff` 按文件段比较（仅统一 CRLF/LF）后，35 个文件段相同；不同的仅 `vision_control.c`、`test_turn_route.c`、`test_equivalence.c`，与前两节记载吻合。其余未提交修改视为用户已有工作，不追认作者归属。
- 对所有下列文件读取了完整当前 diff。起始 `git diff --name-only` 没有 docs 路径，也没有 competition 工程文本差异；报告目录原为未跟踪，不能借 tracked diff 统计声称审完其全部历史附件。

| 文件 | 分类 | 审查结论/关注点 |
| --- | --- | --- |
| `demo/code/app_mode.c` | 既有用户修改（C31 实现，已知范围） | C31 固定 DRIVE、删除无消费者 start_request；不把算法 ROUTE_SWITCH 当启动许可。 |
| `demo/code/app_mode.h` | 既有用户修改（C31 实现，已知范围） | 选择器接口注释与 start_request 删除一致。 |
| `demo/code/app_runtime.c` | 既有用户修改（C31 实现，已知范围） | 采集 stamp 传入跟踪/驾驶；无可信时间则 trip；失败复制不处理。 |
| `demo/code/board_camera.c` | 既有用户修改（C31 实现，已知范围） | 初始化前安装 GPT 采集时钟；风险 R1/R2。 |
| `demo/code/board_io.c` | 既有用户修改（C31 实现，已知范围） | autonomous 不初始化/采样 C26/C27，诊断返回约定电平。 |
| `demo/code/midterm_boot.c` | 既有用户修改（C31 实现，已知范围） | 独立序号、采集/前台双时间连续 3000ms；review、轮速、故障准入。 |
| `demo/code/midterm_route_profile.h` | 既有用户修改（C31 实现，已知范围） | 路线构型允许三标志 C31，保留非 C31 autonomous 拒绝。 |
| `demo/code/vision_actuator.c` | 既有用户修改（C31 实现，已知范围） | 源绑定授权、统一 commit/zero、浮点范围及 PWM 硬上限；风险 R3。 |
| `demo/code/vision_control.c` | 本轮已知修改（前轮/续验，本次未改） | 已知前轮修复：零需求宽限标记 recovering，duty 仍为零。 |
| `demo/code/vision_drive.c` | 既有用户修改（C31 实现，已知范围） | PI 服务接线、C31 观测失效 trip、先撤销输出、限制 UART。 |
| `demo/code/vision_drive.h` | 既有用户修改（C31 实现，已知范围） | 恢复 profile 不授予输出；区分旧延时启动与 C31。 |
| `demo/code/vision_drive_internal.h` | 既有用户修改（C31 实现，已知范围） | 共享 guard/lease/stop volatile 字段、内部授权接口。 |
| `demo/code/vision_frame.c` | 既有用户修改（C31 实现，已知范围） | 前后短临界区快照与序号检查；风险 R1，非 DMA 独占。 |
| `demo/code/vision_frame.h` | 既有用户修改（C31 实现，已知范围） | stamp 契约；稳定性注释受 R1 限制。 |
| `demo/code/vision_guard.c` | 既有用户修改（C31 实现，已知范围） | C31 禁旧 switch_sample；PIT 仅采样/撤销。 |
| `demo/code/vision_motion.c` | 既有用户修改（C31 实现，已知范围） | 输出迁入统一执行器；显式请求授权；autonomous 拒绝。 |
| `demo/code/vision_probe.c` | 既有用户修改（C31 实现，已知范围） | 探针迁入统一执行器；autonomous 拒绝。 |
| `demo/code/vision_profile.h` | 既有用户修改（C31 实现，已知范围） | 派生 C31、非法组合保护、目标标识与 legacy 宏保留。 |
| `demo/code/vision_speed.c` | 既有用户修改（C31 实现，已知范围） | 目标/PI 拆分、一次新样本、积压与时效 fail-closed；风险 R3。 |
| `demo/code/vision_speed.h` | 既有用户修改（C31 实现，已知范围） | 集中 10/20/30/50ms 契约及积压上限 1。 |
| `demo/code/vision_startup.c` | 既有用户修改（C31 实现，已知范围） | C31 不走 legacy deadline/rearm/clear；其他目标统一 arm。 |
| `demo/code/wheel_speed.c` | 既有用户修改（C31 实现，已知范围） | 已存在的按毫秒积分、饱和边界部分增量、10ms 兼容接口。 |
| `demo/code/wheel_speed.h` | 既有用户修改（C31 实现，已知范围） | PI 实际 PWM/积分/目标速度语义说明。 |
| `libraries/zf_device/zf_device_mt9v03x.c` | 既有用户修改（C31 实现，已知范围） | 完成回调发布时间/序号，初始化清元数据；风险 R1/R2。 |
| `libraries/zf_device/zf_device_mt9v03x.h` | 既有用户修改（C31 实现，已知范围） | volatile 元数据与时钟回调声明。 |
| `tests/vision/stubs/main.h` | 既有用户修改（C31 实现，已知范围） | 相机元数据桩声明。 |
| `tests/vision/test_board_mode.c` | 既有用户修改（C31 实现，已知范围） | 消费真实模式事件后允许探针；时钟桩。 |
| `tests/vision/test_debug.c` | 既有用户修改（C31 实现，已知范围） | 发布相机元数据、模拟复制中换帧/ABA；不能覆盖 DMA。 |
| `tests/vision/test_dual_runtime.c` | 既有用户修改（C31 实现，已知范围） | 夹具统一发布完成元数据。 |
| `tests/vision/test_equivalence.c` | 本轮已知修改（前轮/续验，本次未改） | 已知 10-28 harness：输入预定分层、精确行 oracle、解析阴影。 |
| `tests/vision/test_field_protocol.ps1` | 既有用户修改（协议兼容） | 既有用户兼容修改：接受 C26 20..31、拒绝 32/额外后缀。 |
| `tests/vision/test_midterm_boot.c` | 既有用户修改（C31 实现，已知范围） | 消费初始模式事件，显式取消 Boot；与生产调用关系一致。 |
| `tests/vision/test_midterm_runtime.c` | 既有用户修改（C31 实现，已知范围） | 夹具每循环推进 10ms/一次安全采样，timer_get 只读时钟。 |
| `tests/vision/test_speed.c` | 既有用户修改（C31 实现，已知范围） | 既有饱和跨界/零边界回归，未放宽上限。 |
| `tests/vision/test_switch_runtime.c` | 既有用户修改（C31 实现，已知范围） | 夹具发布采集元数据。 |
| `tests/vision/test_turn_route.c` | 本轮已知修改（前轮/续验，本次未改） | 已知前轮恢复修复对应检查；保留 >=995 headroom，确认零/恢复 PWM。 |
| `tools/build_midterm.ps1` | 既有用户修改（C31 实现，已知范围） | auto 别名、宏拒绝继承覆盖、输出路径重写保护、仅 -r 构建。 |
| `tools/field_protocol.ps1` | 既有用户修改（协议兼容） | 既有用户兼容修改：C26 版本模式扩展到 31；没有执行端口工具。 |

补充生成产物/文档检查：

- `demo/mdk/competition/midterm_switch.uvprojx` 起始 status 为 M，但 `git diff --exit-code -- <该文件>` 退出 0，无文本改动；保持原状。
- `demo/mdk/competition/midterm_auto-track.uvprojx` 起始即未跟踪。读取 XML，并从 base 按脚本规则在内存中重现文本，结果一致；没有执行生成器或手写项目。SHA-256：`5B7F30D7B91212D3EB9408190B8E29024152C039D15FA541F0FB6E773AAB63B2`。
- 同样重现 train/route/switch，均一致；SHA-256 分别为 `D9361C9DE4F24B4CC6331879FB30A44AB7457CB907CBBB4410FC81362E4DE36C`、`455B8BC340F5E0069DD6743E6D3CA7AAF29C8831D411743D02FDBC86587497F0`、`2D990CECA40A70CA4736F389620DFF24FB34648069F4EF2711109C43B7C99810`。
- 已读本报告全文、`architecture-status.md`、`macro-audit.md`、`status-before.txt`；两份状态/宏说明保留为历史文档，不覆盖原文。“未验证”等旧状态由本节与前两节结果补充。
- 未跟踪 `tests/vision/test_auto_track.c`、`run_auto_track_tests.cmd` 已补读：review=0/1、连续帧/重复序号/过期采集/超时/STOP/旧入口拒绝等模拟契约在代码中保留；本次没有运行。
- 未对历史附件逐行做新的语义审查：`head-before.txt`、`tracked-before.patch`、`preflight-existing.diff`、`switch-baseline.log`、`switch-diagnostic.log`、`validation/baseline.diff`、`validation/switch-diagnostic-2.log`、`validation/switch-diagnostic.log`、`validation/switch-initial.log`（均相对本报告目录；patch 仅作归属比较/定点搜索）。其他三个历史快照目录也未做全量审查。**本次请求范围内有文本 diff 的 tracked 文件无未审查项**；不声称全仓所有历史资料或所有 SDK 文件已审完。

#### ISR / 前台共享数据与输出路径证据

1. **调用关系和单一职责。** `demo/user/src/isr.c:50-53` 的 CSI IRQ 调 SDK；`isr.c:72-77` 的 PIT1 先采模式再调用 `vision_drive_safety_tick()`。实际安全函数 `demo/code/vision_guard.c:3-29` 采样编码器、检查运动/探针到期、guard 租约并归零，不授予授权。前台 `app_runtime.c:253-256` 调 drive poll 和 Boot poll；`vision_drive.c:205` 服务 PI；`vision_drive.c:219-223` 在帧间提交持有的 PI 输出。PIT 配置见 `vision_drive.c:138-139`，源码设置优先级 0；并未实测优先级/屏蔽时长。ISR 的旧“no encoder/PID use”注释与实际编码器采样不完全一致，应以调用代码为准。

2. **相机元数据发布与快照。** `board_camera.c:8,16-17` 在 init 前安装 GPT1 回调；`demo/user/src/main.c:41-42,54` 先启动 GPT 再 board init。底层 `zf_driver_timer.c:76-78` 读取 GPT 计数。驱动 `libraries/zf_device/zf_device_mt9v03x.c:393-423` 取 full buffer、更新 image 指针、回收另一缓冲，随后发布 capture_ms/time_valid、sequence，最后 finish_flag；`:435-441` 初始化清元数据，`:484-490` 安装回调并启动 CSI。元数据为 volatile，时钟函数指针在采集启动前设置，正常运行不改。
   `vision_frame.c:11-19` 在 PRIMASK 下取得指针/元数据并清 finish_flag，`:20` 开中断 memcpy，`:21-25` 再屏蔽检查 finish_flag、指针和序号。在“每次缓冲回收均被回调发布，IRQ 可及时完成”的前提下，复制期间回调发生即拒绝；序号还覆盖指针 ABA（不含 2^32 次发布回绕）。复制可能读取混合数据，但检测到软件发布变化时该副本不会进入处理；`app_runtime.c:261-263,277-296` 仅成功副本处理，且只有可信 stamp 才驱动。

3. **R1：DMA 缓冲所有权仍不能无条件证明。** `zf_driver_csi.c:74-93` 只是 SDK submit/get 包装；SDK `libraries/sdk/drives/fsl_csi.c:883-900` 先入 full 队列、回调，`:913-927` 随后重新装入空缓冲或复用另一地址。MT9V03X 回调立即交回“另一缓冲”，没有前台持有/释放协议。`fsl_csi.c:867-870` 在两个 DMA done 标志同时出现时直接跳过，不递增采集序号。PRIMASK 不停止 DMA，DMB 不授予缓冲所有权。IRQ 延迟、双完成跳过或硬件已开始覆盖但尚未发布的情形，不能由序号稳定推出无混合帧；主机 COPY_RACE/ABA 桩只验证软件发布，不能证明这一点。此为明确的证明缺口，不能通过给共享标量加 volatile 来解决；需独占缓冲/可靠的 DMA 状态协议，或可验证的 IRQ 与复制时限依据，才可关闭。此次没有硬件依据来选择并验证采集协议改动，保留源码和风险。

4. **R2：时间戳不是曝光时刻证明。** capture_ms 是完成回调执行时读取的 GPT 时间；time_valid=1 只证明安装了时钟函数。尚无硬件锁存完成时刻，延迟 ISR 可能把旧图标成较新的“完成回调时间”。相机时钟频率、DMA/CPU 可见性、DTCM 映射与缓存一致性、漏中断、32 位访问和回绕边界不能仅凭主机证明。未引用或推断未经核实的 MCU 原子性。

5. **授权、零输出、缓存和 PWM 顺序。** 实际 `pwm_set_duty` 搜索覆盖 `demo/code` 与 `demo/user`：运行时调用只有 `vision_actuator.c:15-17` 的三路零及 `:36` 的统一缓存写；初始化另有 `demo/user/src/main.c:23-27` 的三路 `pwm_init(...,0)`，底层 `zf_driver_pwm.c:213` 再调 set_duty。授权授予只在 `vision_actuator.c:119-149`，C31 禁止非 local 和诊断入口。`:7-18` 归零顺序为撤销 lease/guard、清缓存、重置 PI、三路实际写零、DMB 后恢复原 PRIMASK。`:153-168` commit 再查授权与时效、限幅，先 servo 再两电机；`:198-202,301-310` 目标读取至最后 PWM 写及 guard 快照发布都在同一屏蔽段。相同 duty 缓存仅在写入后更新；STOP 总写硬件并清缓存，不能把“缓存相等”当作已执行 STOP。
   因此对于代码中的可屏蔽 PIT ISR：若归零先完成，后续前台 commit 必须看到授权撤销；若 commit 先完成，后到的 PIT 归零会覆盖它。单纯观察新视觉帧不重新授予 lease。`vision_drive.c:267-315` STOP/trip 先撤销再处理控制状态；C31 STOP sticky，`:323-324` 拒绝 UART arm/clear；`vision_startup.c` 编译隔离 legacy 重新授权。
   底层 `zf_driver_pwm.c:114-117` 是清 LDOK→更新缓冲 duty→置 LDOK；源码证明的是寄存器写请求顺序，**不是多通道同时生效、物理即时归零或停车距离**。

6. **共享状态与 PI。** `vision_drive_internal.h:16-34` 的 motion、guard、授权、停止、probe 共享字段有 volatile；缓存数组不靠 volatile，靠输出临界区串行化。`vision_speed.c:10,21-28` 的 speed 为 volatile 整体；PI、consumed、last_control、sample_interval 和目标请求采用临界区而非逐字段 volatile。`:88-89` speed_get 是屏蔽快照，`:262-267` PI 服务在屏蔽段，执行器发布目标在自身屏蔽段，PIT 仅采样/故障触发 reset。新样本按 `:133,147-166,220` 消费一次，积压 >1、间隔不在 10..20ms、年龄 >30ms、活跃控制停顿 >=50ms 均有 fail-closed 路径；不重放同一测量。
   `vd.control`/profile/Boot 连续帧状态主要由前台持有；PIT 正常 DRIVE 只读已发布 guard 快照，避免读控制器多字段中间态。诊断 motion 分支会检查 control.state，但不授予非零输出；其时间/phase 更新在前台屏蔽段。初始配置发生于零输出且未授权阶段。
   **R3：** 上述证明依赖编译器对 CMSIS 屏蔽/屏障内存语义的实现、PRIMASK 对实际 IRQ 的覆盖；未审反汇编或证明所有非屏蔽异常。临界区包含浮点/PI/PWM 寄存器写，未有 MCU 最坏执行时间证明；可能延迟 PIT/CSI。没有声称 volatile 自动提供复合原子性，也没有声称前台 PI 已成为硬实时调度器。legacy 模式多字段读用于显示/先验判断不等于原子快照，最终输出仍在屏蔽段重查。

7. **启动不变量。** `midterm_boot.c:18-39` 必须以不同序号的新鲜有效近远观测推进，采集时间与前台时间都连续覆盖 3000ms；同帧 poll 不积累资格。`:73-85,106-119` 将 sticky STOP、guard/controller/encoder fault、review 与速度准入纳入最终判断，arm/apply 在同一外层屏蔽段。默认 `midterm_profile.h:31-35` review=0，当前 auto-track 工程也显式 0；软件不能替用户确认视觉/轮向，更不能证明跨 MCU reset 的断电锁止。

#### 构建策略、宏与工件核验

`tools/build_midterm.ps1:3-6,12-18` 定义别名及宏矩阵；`:30-57` 拒绝基底受控宏并检查 define/output/listing 重写；`:66` 唯一 UV4 参数为 `-r "<project>" -t "nor_sdram_zf_dtcm" -j0 -o "<log>"`，build-only，无 download/flash 参数。`:79-83` 检查 AXF、零错误/警告与本次构建起点后的时间戳。静态解析 base 及四个生成工程：每个只有一个 target，所有 BeforeMake/AfterMake/BeforeCompile 的 RunUserProg1/2 均为 0；未执行工程或 UV4。

| 入口 | training / encoder | autonomous / route_trial / route_switch | auto_track / start_switch / visual_reviewed |
| --- | --- | --- | --- |
| train | 1 / 1 | 0 / 0 / 0 | 默认0 / 默认0 / 默认0 |
| route | 1 / 1 | 0 / 1 / 0 | 默认0 / 默认0 / 默认0 |
| switch | 1 / 1 | 0 / 1 / 1 | 默认0 / 默认0 / 默认0 |
| auto-track | 1 / 1 | 1 / 1 / 1 | 1 / 0 / 0 |
| auto | 同 auto-track | 同 auto-track | 别名；写入 auto-track 目录 |

宏消费者再次核对：`vision_profile.h:10-20` 的 AUTO_TRACK 编译约束与 C31 三标志派生；`:191-195` 的 legacy auto/start 默认；`vision_drive.c:112-120` 消费启动 profile；`vision_startup.c:3-23,76-97` 消费旧调度（autonomous 已隔离）；`midterm_profile.h:35` 及 `tools/verify_project.ps1:34,40` 消费 START_SWITCH 编译/工程保护；`midterm_boot.c:108` 消费 review。不把目标标记冗余当无用途，也没有证明不存在外部构建含义；本次不删任何宏。

以下为只读 SHA-256 和文件系统 UTC 修改时间（`computed`）。四个当前目录 AXF 均晚于本次 tracked diff 中全部生产源码及 build 脚本的修改时间，日志有 0 Error(s), 0 Warning(s) 且紧随 AXF；这只验证时间新鲜度和当前字节身份，不等同可重复构建/二进制与每个输入内容的密码学溯源，前轮构建前哈希未记录，不能补造。首次 auto-track 构建日志也保留；其 AXF 后来由 auto 别名构建覆盖，无法从当前文件恢复首次 AXF 哈希。

| 文件 | 字节 | 修改时间 UTC | SHA-256 |
| --- | ---: | --- | --- |
| `.build/midterm-auto-track/Objects/rt1064.axf` | 1172292 | 2026-10-27T13:50:02.9908289Z | `580C0307ED31CC615B58623E2853D72F42C0313FAD4C5514B0BCE14BF2035E99` |
| `.build/midterm-auto-track/build-20261027-214948-974.log` | 5860 | 2026-10-27T13:50:03.8222010Z | `F7139E9D47EC7A1DA2996E4759ACF4BD8490EEB9226A72E8398FCE86939D255B` |
| `.build/midterm-train/Objects/rt1064.axf` | 1111588 | 2026-10-27T13:49:05.5413078Z | `EC0524284D52CD15182DB22C75998E2723ED269A969E8C69FF7D5BFD17D99B9A` |
| `.build/midterm-train/build-20261027-214848-411.log` | 5855 | 2026-10-27T13:49:06.3122944Z | `2E80C0732B364C8C50FDFB9C910C0EFCE1F136A11F2E487C43C30C67DEE4DBBA` |
| `.build/midterm-route/Objects/rt1064.axf` | 1113096 | 2026-10-27T13:49:28.2572637Z | `42EF5A36B91C35335BBBB034FF1F8FA2EAF693763DF66366A2B827ED7715E8B8` |
| `.build/midterm-route/build-20261027-214907-399.log` | 5855 | 2026-10-27T13:49:29.0299690Z | `B5C8A16FD8CE86B8D83C267EE1019E9E87BDD22CFBD0BAF432C4961EA9467E93` |
| `.build/midterm-switch/Objects/rt1064.axf` | 1201560 | 2026-10-27T13:49:47.1917988Z | `2F1B4AA79112D19BD1EA78BF6A6B83A0A7562987F28D990FC3EB7B9D4FABA4BC` |
| `.build/midterm-switch/build-20261027-214930-206.log` | 5856 | 2026-10-27T13:49:47.9374599Z | `7E97B1C646F585A725657A311C5723C331DC37C8D2B4FC28D86DC07C61390818` |
| `.build/midterm-auto/Objects/rt1064.axf` | 1062624 | 2026-10-20T15:23:48.5010329Z | `567635DE083024988095892B63A7CCD4FEB73A519A11CFAD06EAD7BB6CA816C2` |
| `.build/midterm-auto/build-20261020-232337-939.log` | 5850 | 2026-10-20T15:23:49.0307521Z | `8A6FA9A1836CC849F3AC56791366B60B3775149274236473C30C5CE4E0B2112F` |
| `.build/midterm-auto-track/build-20261027-214826-140.log` | 5860 | 2026-10-27T13:48:47.1959106Z | `8178909B3940C8CF6FD382DD39C7CDFEAD8758F59FBA819954D694C7583F5076` |

**发布目录风险：** `.build/midterm-auto/` 的 AXF 和最新日志均停留于 2026-10-20，早于全部 26 个已修改生产/脚本文件；它是旧 autonomous 产物，不是第五次当前构建产物。2026-10-27 的第五次 `-Variant auto` 输出是 `.build/midterm-auto-track/`，对应最后的 `build-20261027-214948-974.log`。旧目录仅记录与标记过期，没有删除/覆盖，也没有把旧 AXF 当成已通过的 C31 发布件。

#### 命令结果、未审项与结束自查

- 本次未运行任何 host suite、Keil build、flash、串口、硬件或实车命令；之前真实退出码仍由上文保留，未冒充本次执行。
- 各逐文件 `git diff -- <paths>`、定点源码读取、修正后的 `rg -g` 搜索、Get-FileHash、工件存在/时间检查均退出 0；内存生成工程复现/禁用钩子断言及按文件段归属比较的 Python 检查退出 0。
- 两次定点搜索曾错误地把 Windows 路径通配符直接传给 rg（`vision_*.c`、`board_*.c`），各退出 1，随后改用目录加 `-g` 完成搜索；一次归属比较直接 UTF-8 解码含旧编码驱动的完整 diff，退出 1（UnicodeDecodeError），随后改为字节分段比较退出 0。这些是只读审查命令错误，不是测试失败或沙箱失败。
- 未审查清单限于上列历史附件的逐行语义、全仓无关文件、SDK 其他路径及最终机器码；R1/R2/R3 均未被主机或本次静态审查消除。没有连接硬件来“补证”。
- 本次新增修改应仅为本报告追加内容；既有 38 个 tracked diff、未跟踪测试/工程和历史资料逐字节保持，工件复核哈希保持。最终 Git 命令与保留检查结果见本节末尾的完成记录。
- 自查：仅在批准项目中定点读取/写入；没有读凭据、改 Bridge/Codex 配置、建立持久化、访问 `.build/video-deps`、递归扫描全仓、提交或破坏性 Git 操作。没有修改安全阈值、PI 增益、编码器符号或默认 review 门。审查完成与硬件时序已证明分开报告。

完成记录：追加后执行 `git diff --check`、`git status --short --branch`、`git diff --stat`、`git diff --cached --stat`、`git diff --cached --quiet`，各退出 **0**；diff 仍为 **38 files / 837 insertions / 202 deletions**，cached stat 无输出，暂存区为空，status 条目与起始相同（报告本身仍在既有未跟踪目录中）。只读保留检查退出 **0**：除本报告外的 **65** 个起始文件 SHA-256 全部不变；**11** 个 AXF/log 哈希复核全不变；原报告前缀与起始哈希匹配。所有本次文件写入只发生于本报告，没有新增源文件或临时产物。工程 XML 使用 `structured_parsed`，文本源码/Markdown 直接读原文件，哈希、时间比较和工程重现为 `computed`；无仅靠 `md_extracted` 得出的精确数值，也未处理 PDF 或声称 `pdf_verified`。本段落写入后再次复核上述五条最终 Git 命令，不执行构建或测试。
