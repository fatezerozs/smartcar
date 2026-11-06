# MPC_20261106：项目、文档与标定数据核定

日期：2026-11-06，Asia/Shanghai。按用户要求，先核定事实、修正发现的问题，再归档推送到GitHub。发布目标为当前开发分支`feat/vision-recorder-training`，正式维护分支`main`不在本轮直接合入范围。本记录是开发交付核定，不是整车或烧录放行。

[工程导航](../PROJECT_INDEX.md) · [项目现状](../PROJECT_EVOLUTION.md) · [机器证据](2026-11-06_MPC_AUDIT_AND_PUBLICATION.json)

## 1. 核定结论

| 对象 | 核定结果 | 边界 |
| --- | --- | --- |
| 工程命名/入口 | 主工程为`MPC_20261106.uvprojx`，构建/审计工具与文档入口已同步 | MPC是当前工程名；预算规划器尚未接入主固件 |
| CPR与方向 | 左2335.8 counts/轮、前进raw乘数−1；右2358.6、乘数+1，与原始计数、采用记录和生产宏一致 | 已采用不等于计量重复性认证；另一轮跳变与各次差异原样保留 |
| 原始Encoder数据 | 9窗口、4500次10ms采样、7组手转比值重新核对；长度/CRC/序号/模差/累计值及70文件SHA256通过 | 窗口位于手转前后，不能虚构未记录的手转速度轨迹；圈数为操作者报告 |
| 几何与舵机 | 64mm轮径是用户图中的标称值；轴距205mm、轮距152/150mm为图像估算；1430/1550/1670µs为既有静态命令端点 | 均不自动成为负载周长、真实轮角、相机地面标定或动态性能 |
| 动态与降级阈值 | Host v0.3保留来源明确的参考值，`productionApproved=false` | 制动、jerk、延迟、near距离、far时距预算仍未由本车完整数据定标 |
| B2/B3/SD实测 | 既有局部实测与原报告相符，范围见工程导航；原始记录和冻结清单不改写 | 独立通过不能相加成联合通过；联合Camera/SD/C26、运动会话与赛道验收仍缺 |

本轮没有重新手转、连接串口、烧录、复位或恢复动力；核定来自现有原始数据及既有构建/测量记录。

## 2. 已更正的问题

### 2.1 启动时长与恢复策略描述

README中“auto-track连续3000ms准入”已更正为当前源码的1000ms；普通历史构型的3000ms描述按原适用范围保留。主C26启动入口没有改为AUTO_TRACK启动器。

用户明确：**AUTO_TRACK与RecorderTrain后续都要贴合MPC预算降级。** 两者不再以rev5的无预算near-only持续驱动作为未来目标。当前共享C31源码仍有零动力恢复窗口，预算尚未集成，因此本轮将其明确为**过渡保护**，没有给生产代码填入猜测的降级速度或时距阈值。

原reviewed测试期待near-only/partial持续非零输出，实际源码则清零并在期限到达后锁止。修正时保留输入与场景，具体变化为：

- `test_auto_track.c`保留near-only，分别验证窗口内零PWM/零目标/零PI缓存、及时恢复，以及新增的持续失效到期锁止；partial仍不得重新供给动力，STOP与撤权场景保留。
- `startup_harness.c`保留scenario 19的near-only图像输入，改为验证当前过渡策略的零输出、VISION故障与停止；scenario 16/17短时失效恢复及20/21中途STOP/export仍独立执行。

这些通过只说明当前过渡实现与其测试一致，**不计作MPC有预算非零降级已经完成**。未来接入MPC预算监督时，必须同时更新源码、构型能力声明和对应行为测试，不能只扩大恢复窗口或删除失败场景。

### 2.2 Recorder源码锁

从既有安全审计ZIP恢复了锁定哈希`572d1a38d35eff1911db3c7fa8ce809051459f56eee5e732918d3a7b87dcd2c6`对应的原始`vision_drive.c`。与当前源码逐字节对照，仅有一处替换：

```c
vision_drive_encoder_signs(-1,1)
// 改为
vision_drive_encoder_signs(MT_ROUTE_ENCODER_LEFT_FORWARD_SIGN,
                           MT_ROUTE_ENCODER_RIGHT_FORWARD_SIGN)
```

已核对当前宏仍为左−1/右+1；因此审查后更新`demo/recorder/sdk_sources.lock.json`中该项，其余SDK输入锁不变。没有跳过锁检查，没有重新计算一批未知来源哈希来掩盖差异。Recorder构建准备、重复生成和默认review=0契约复测通过。

### 2.3 GitHub可复现性和证据字节

RecorderTrain、B2和B3的生成Keil工程引用私有适配源或绝对run路径，不能从干净克隆直接使用。它们改为由版本化脚本生成，不提交本机生成工程；当前导航链接改为对应构建脚本。主MPC工程、基础/route/auto-track及独立SD源工程仍保留直接入口。

`.gitattributes`补充冻结证据/标定交付的原字节规则，避免Git自动换行转换破坏已有SHA256。实际从Git暂存区导出后，70个交付文件与原始工作区逐字节相同，CRC与参数审计再次通过。MPC工程XML也按原字节保存。

`.gitignore`排除临时EXE/DLL、含本机run路径的生成工程及48.125MiB原始SD压力回读载荷；它们保留本地，不删除。SD校验报告、SHA清单和命令日志发布。明确版本的B2被测AXF，以及SD Diagnostic交付AXF/BIN/MAP作为小型归档例外保留并核对已有SHA；这些是历史交付证据，不是新MPC发布镜像或自动烧录对象。

历史快照里的旧路径与旧失败不改写。活动文档到本机`.test-results`的链接改为明确的本地路径，公开摘要仍可访问；冻结的`before-*`文档作为原字节证据，不作为当前导航。

首次推送前发布副本的649处活动文档链接检查通过，两份基线文档中的9处本机绝对链接已转成仓库相对链接。当时功能源码、测试、工具和参数输入与通过完整Host验证的暂存副本一致；随后远端MSVC发现的差异及更正单列于2.4。活动源码/脚本/测试/参数的增量空白检查通过；原始构建日志、MAP和历史补丁保留原有尾空格，不为外观检查改写证据。

### 2.4 首次推送后的MSVC兼容性更正

首次推送提交`d01df9a47d3ceee1e79939e08418625f09982b40`触发[Host regression #4](https://github.com/fatezerozs/smartcar/actions/runs/42212004325)。optimized和ASan均为46 PASS、2 ERROR、1 FAIL、1 SKIPPED：auto-track和recorder-startup在`vision_control.c`触发C4702不可达代码，speed-planning-host在测试数据初始化触发C4244整数到浮点转换。错误发生在MSVC `/W4 /WX`编译阶段，不能写成远端通过，也没有降低告警等级绕过。

更正为：C31的`c31_zero_wait`分支已经返回，仅在非C31构型编译后面的旧兜底语句；测试曲率初始化使用`0.0f/2.0f`。没有调整降级期限、输出规则或生产标定值。首次推送与后续更正的运行记录分开保存，后续实际结果见[CI更正证据](2026-11-06_MPC_CI_COMPATIBILITY.json)。

更正后本地完整PreFlash重新执行：`20261106T114440-8e894432c2`，63 PASS、1 SKIPPED、382.413秒，九种Keil构型和ARM编译再次通过，所测源码SHA与更正文件一致。该记录不替代后续推送的远端MSVC结果；远端结果必须按实际提交SHA核对。

第二次推送`ad215e5`的[Host regression #5](https://github.com/fatezerozs/smartcar/actions/runs/42213381153)继续执行后，optimized为47 PASS、1 ERROR、1 FAIL、1 SKIPPED：auto-track及基础规划已通过，Recorder的reviewed变体出现C4389有符号/无符号比较，组合回放进入后出现C4996 CRT `fopen`告警。再次更正仅涉及测试：将运行标志先转换为逻辑表达式，MSVC下使用`fopen_s`、其他编译器保留`fopen`，继续检查打开失败。没有屏蔽告警、减少场景或更改固件；专项Host及依赖`20261106T115628-c46e55a1e3`为8 PASS。完整PreFlash所覆盖的生产代码未再次变化，两份测试程序以该专项输入哈希补充覆盖。

## 3. 实际软件验证

| 检查 | 本轮实际结果 |
| --- | --- |
| Manifest/Schema/路径/DAG | 72个定义校验通过 |
| 原3项失败专项及依赖 | `20261106T111524-2193f1c6fa`通过，包含auto-track、recorder-startup、recorder-tools |
| 完整PreFlash，工作区 | `20261106T112050-ad34668c75`：63 PASS、1 SKIPPED，295.495秒；ARM编译和九种Keil完整链接通过 |
| 仅Git暂存文件导出的完整Host | `20261106T112627-10ffe28ffc`：49 PASS、1 SKIPPED；不依赖原工作区的未提交生成文件 |
| 标定证据 | 原始工作区、暂存导出副本均通过70文件哈希、9窗口、4500采样、7组比值核对 |
| 文件/文档审查 | 凭证模式检查、发布文件范围、工程引用、活动文档链接、冻结字节和增量空白检查；摘要见机器证据 |

两个完整计划均为`softwarePassed=true`、`releaseEligible=false`：本机选择GCC14.2，不能把它写成已完成本批MSVC/ASan CI或正式放行。SKIPPED均为缺数据的`performance-host`，不阻塞其他模块；不生成虚假的准确率或性能改善值。

验证使用提交前dirty工作区/暂存导出副本，记录中的Git SHA属于当时父提交，具体测试输入由源码哈希界定。原Phase 3的46 PASS/3 FAIL/1 SKIPPED保留为此前状态；本页是其后更正结果，不能把两个运行记录混为一次。

## 4. 当前仍待完成的工作

1. Phase 2：左右负载滚动周长、实际轮角、相机地面映射/盲区、动态响应/制动/总延迟，以及同步、可审核的运动Session。CPR已采用，不重复写成未知。
2. Phase 3：将同一MPC预算规划/监督接口接入MPC、AUTO_TRACK和RecorderTrain各自的授权/调度入口，完成一致快照、候选复核、短提交区和受控停止。保留C26主构型与独立启动器的职责边界。
3. 联合静止、正常运行到OFF的实测切断、控制与SD并行时序、真实A/B、误停/圈时及整车赛道验收。
4. 远端MSVC optimized/ASan是推送后独立结果；在远端任务实际完成前，本页不预先宣称其通过。PreFlash自托管工作流与任何硬件验收也不因push自动放行。

这些是明确未完成的集成/实测门，不以猜测数据补齐；它们与本轮已修正的软件/文档失配分别记录。

## 5. 发布范围与读取层

本批归档包含本工作区累计的采集框架、C31安全边界、速度规划Host工具、已采用Encoder参数、测试框架扩展、原始标定窗口及工程文档。保留上游SDK与FreeRTOS许可；不提交个人IDE状态、私有构建目录、临时可执行文件或未核定的自动生成项目。具体文件与摘要以Git提交和本页机器证据为准。

发布到`feat/vision-recorder-training`，不强推、不删除远端分支、不改写`main`或历史发布。远端原有`main`上的`1103`文件不受本次推送影响。提交/远端SHA以实际Git历史为准，避免在提交内容里伪造尚未产生的自引用SHA。

读取层：原生C/Markdown直接读取；JSON/XML/CSV/B3T1为`structured_parsed`；CRC、模差、CPR、SHA256、Git导出对照和实际测试/构建为`computed`。几何图来源沿用原图核验记录；不以`md_extracted`单独确立精确数值，无新增`pdf_verified`或物理实测。没有修改全局配置、技能或持久Agent行为；项目内`.gitattributes`、`.gitignore`的修改仅用于本仓库的证据保存与发布范围。
