# 验证、构建及逐项交付记录

开始：2026-11-05；收尾：2026-11-06（Asia/Shanghai）。任务版本v1.4。

**总体验收未通过，尚未接入生产控制路径。** 无本车标定数据/真实session，不能越过Phase 2参数审查门。此轮交付的可运行代码在 `tools/speed_planning` / `tests/planning`；没有修改生产源码、Keil宏、驱动、通信、存储或硬件接口。

## 1. 基线身份与保护

HEAD de1415743b004160f520d385c9e3e63008f52007（main）；开始时存在未提交安全整改/Recorder/PC数据工作。不是clean-main验证。原1015文件SHA-256和开始前binary diff保存在 `.build/speed-planning`。

原有文件仅5个发生本轮增量：主页README、文档INDEX/PROJECT_EVOLUTION、套件README、catalog；保留了这些文件中先前未提交的内容。其他原文件逐字节未变，包括所有demo/libraries生产文件和旧测试断言。catalog原70项原样保留，新增2项，共72项；旧C32手册66项索引是当时归档，不是当前registry数量。

本轮自身diff保存在 `.build/speed-planning/task.patch`，增量文件清单及逐文件审阅见 `.build/speed-planning/task-review.json`。不把开始前其他人的变更计入本任务，不执行git add/commit/push，也未尝试远程CI。

## 2. 实际测试结果

| 运行 / 范围 | ID | 结果 |
|---|---|---|
| 开始前完整Host，GCC optimized / Jobs2 | 20261105T153030-32177ad296 | 45 PASS / 2 FAIL / 1 SKIPPED；84.049s |
| 新原型+C26现状专项及关联回归 | 20261105T155130-5aeb3913f6 | 23套件节点PASS，softwarePassed=true；不是完整Host/PreFlash |
| 最终原型+C26专项重测 | 20261105T160128-6d11e04b6a | 4节点PASS（含2个环境前置）；softwarePassed=true，releaseEligible=false |

关联回归包括Speed/encoder、Link/Link-board、Equivalence、Route-switch/Switch-runtime/Turn/Roundabout/Projection/Tangent、framework、project/static、合法/非法宏契约，保留原断言。

新增Host原型：8个Python数据契约/episode测试；C共104496次CHECK，其中100000次是固定规划输入的重复执行/CPU计时，不能称104496个独立场景。独立Python oracle对照6×16点；500点时间轨迹；状态机边界、噪声传播、目标映射、数值无效/回绕等见单独 [性能报告](NORMAL_PERFORMANCE.md) / [安全报告](SAFETY_REGRESSION.md)。

### 开始前既有失败

- `auto-track`：reviewed变体，`tests/vision/test_auto_track.c:113`，断言 `midterm_boot_state()==MT_BOOT_RUNNING&&vd.control.output.observation_valid&&!vd.control.output.far_valid` 失败。
- `recorder-startup`：reviewed场景，`tests/recorder/startup_harness.c:113`，断言 `saw_running && saw_pwm && vision_drive_is_local()` 失败。
- `performance-host`：原有缺数据的optional SKIPPED。

失败来源是开始前工作区的源码/测试组合，本轮原封未动，没有跳过、删断言或把它们改成通过。二者是AUTO_TRACK/独立训练构型，不替代MIDTERM_C26_31主验收，也不能因此声称完整Host全绿。

MSVC/ASan未执行；本机使用已有GCC14.2.0。脚本支持共享runner传入的MSVC/mode，但不把未执行路径写成已通过。

## 3. 主构型实际工程纯构建

项目：`demo/mdk/competition/midterm_switch.uvprojx`，目标nor_sdram_zf_dtcm；完整目录私有副本，仅重定位OutputDirectory/ListingPath，hooks原本均关闭。没有以base派生宏工程或Recorder工程替代。

首轮 `.build/speed-planning/actual-switch`：0 Error(s), 0 Warning(s)，Keil报告13s。
Code=89672、RO=13792、RW=212、ZI=196648 bytes，注意这是本工作区完整AXF，不是新原型成本。

| 产物 | bytes | SHA-256 |
|---|---:|---|
| rt1064.axf | 1209152 | b36d635e4371de54d2eac667261e4ee03a62e165bdf11c797c9341ce37bf40d9 |
| rt1064.bin | 110336 | d7f1bb403f0d30cabd107ca276a419db78ccb0ecf99d982c336779bb9f4f7c82 |
| rt1064.map | 2061579 | 88f822fda063a5e0ea024ebdc155dbe16a5420def32df2fb84a5668ceec43638 |
| fromelf disassembly.txt | 2549766 | 2dffc9e5a517c8ce732681d2e772a4517810fc3195ad797e8b98cea762036b0e |

第二轮实际运行随交付的复现脚本，输出 `.build/speed-planning/repro-switch`，同样0错误0警告；其哈希单独记入evidence.json。`__DATE__/__TIME__`和路径导致两次二进制/调试哈希可不同，不宣称可重复二进制构建。

```powershell
# 输出目录必须不存在；脚本拒绝覆盖。只使用已有工具链。
python tools/speed_planning/audit_build.py `
  --output .build/speed-planning/new-switch-build `
  --uv4 E:/ElecTronic/Keil/UV4/UV4.exe `
  --arm-bin E:/ElecTronic/Keil/ARM/ARMCLANG/bin
```

### 宏与构型差异

实际工程XML、ARM预处理宏清单、MAP/AXF符号相互核对。主要值：

| 项 | switch主构型 | Recorder独立工程/生成器（辅助静态检查） |
|---|---|---|
| VISION_MIDTERM_AUTONOMOUS | 0 | 1 |
| VISION_MIDTERM_ROUTE_TRIAL / VISION_ROUTE_SWITCH | 1 / 1 | 1 / 1 |
| VISION_ENCODER_ENABLE / VISION_MIDTERM_TRAINING | 1 / 1 | 1 / 1 |
| VISION_FIELD_BUILD | MIDTERM_C26_31 | 不用其标识替代主验收 |
| 启动owner | vd_route_switch_poll | 独立Recorder/AUTO_TRACK流程 |
| autostart_enabled | 宏默认1，route RAM覆盖0 | 以其专用流程为准 |
| 启动delay宏 | 2000U，不是route-switch实际延时 | 不迁移为主启动路径 |
| max_run_ms | route RAM覆盖0 | 独立配置不能替代far预算 |
| fast ticks配置 | 132 ticks/10ms | prepare_recorder生成替换为40 |
| SD_ENABLED | 存在 | 存在；运行调度不同 |
| 持续SD任务 | 未启动，最终AXF不保留Recorder任务/f_write/SD_WriteBlocks | 专用RTOS工作流，仅辅助数据采集，不参与主线运行 |

预处理还确认frame timeout100U、旧恢复INITIAL300U/PARTIAL_FRESH100U/MAX1200U、电机PWM17000Hz；舵机main.h为50Hz、PWM_DUTY_MAX=10000。它们不是新的降级预算或物理制动参数。

### 指令审计

首轮AXF中输出/速度/开关函数入口和固定指令统计：

| 函数 | 地址 | 当前反汇编段指令数 | CPSID / PRIMASK恢复 | 算术浮点指令数 |
|---|---|---:|---|---:|
| output_commit | 0x80009e38 | 62 | 0 / 0；依赖调用者 | 2 |
| vd_output_apply | 0x8000e250 | 317 | 1 / 6 | 21 |
| vd_output_zero | 0x8000e880 | 26 | 1 / 1 | 0 |
| vd_route_switch_poll | 0x8000e920 | 119 | 3 / 3 | 0 |
| vd_speed_control_poll | 0x8000ea64 | 650 | 1 / 3 | 18 |
| pwm_set_duty | 0x8000a4f0 | 47 | 0 / 0 | 0 |

统计为静态函数段，**不是某条动态路径执行条数或关中断时长**。目标编译器内联/重排后output_commit段仍有浮点操作；不能说最终区间全是整数。主构型PI计算和apply尚有长PRIMASK，属于生产整改待完成。零写顺序、缓存失效、PWM缓冲寄存器/LDOK由源码及对应目标指令核查；实际reload延迟、ISR/DMA/WCET均未硬件验证。

## 4. 原型资源（未链接到主AXF）

ARMCLANG18.0.0，Cortex-M7 / fpv5-d16 / hard-float / O2纯对象编译通过；fromelf报告planner对象Code6350（含literal144）+RO80，RW0/ZI0 bytes。库无静态状态，状态由调用者分配。

GCC Host sizeof：preview144、history148、plan136、trajectory12、supervisor72 bytes；不能不经目标ABI核对就当成最终MCU布局。

ARM `-fstack-usage`自身静态栈帧：geometry344、plan216、shape40、wheel0、servo8、supervisor_init8、arm16、supervise40、candidate_check0、vision_adapter184 bytes。adapter会调用geometry，因此不能用max(344,184)当整条路径栈上界；尚需libm/调用链/RTOS/中断叠加。

Host批量100000次规划在最终测试约.045s CPU时间；只是粗粒度批量观测，不是逐次最大耗时。未测目标WCET、实时任务负载、最坏堆栈或实际FPU竞争。

## 5. C01–C09 与阶段门

| 条目 | 本轮交付 | 未完成 / 不能验收 |
|---|---|---|
| C01 | 源码/调用链/单位、开关及Storage审计，实际宏/MAP/AXF/反汇编 | 物理脉宽→角度、停车/转矩、运行时间测量 |
| C02 | 复用原生result的固定数组适配、normalized/metric区分、曲率及新鲜度Host测试 | 真实地面标定、独立near/far质量阈值、真实session及目标WCET |
| C03 | 空间上限/可见终点、前后向传播、原因与Python oracle | 本车参数、实际路径回放、生产接入 |
| C04 | 离散jerk、独立cap/不可行标志、硬STOP参考清零、曲线 | 实测反馈闭环接入、实际制动能力及生产时序 |
| C05 | 明确前轮转向/后轮驱动，Host非对称舵机表及校准受控分配接口 | 真实转角/轴距/轮距/轮径/counts与电机标定，实际PI接线 |
| C06 | Host near/far/controlled-stop状态、时距独立累计、统计脚本 | 真实P99/迟滞/near-only上限/减速度，生产策略 |
| C07 | 原主线授权/指令审计；Host代际候选测试 | 真正最终PWM/PI抑制、Control快照/短临界区和交错集成 |
| C08 | 可选版本化PC sidecar/parser、session统计与兼容拒绝 | 板端有界队列/Storage唯一消费、溢出时序/完整率试验 |
| C09 | 正常/安全分开报告，已有关键关联回归/主构型纯构建 | 真实同输入主线A/B、跟踪/误停/圈时，完整Host既有失败及正式放行 |

Phase 0软件审计已交付；Phase 1形成可运行Host参考实现；Phase 2只有合成输入验证，真实阈值未定标/审查；Phase 3未开始生产接入；Phase 4的软件辅助检查部分完成。不能把这张表写成全打勾。

## 6. 读取、计算和风险纪律

输入本身为UTF-8 Markdown，直接原生读取；环境无MarkItDown MCP。源码同为原生文本；Keil XML、catalog/report JSON为structured_parsed；哈希、C/Python对照、构建和指令统计为computed。无布局敏感的PDF/Word数值来源，无pdf_verified或硬件证据，也没有将md_extracted当物理结论。

本轮没有修改全局配置、skills或持久代理规则。最终自查聚焦生产源码/既有工作逐字节保护、增量diff、文档链接/图示/数字来源及未验收项。仓库整体 `git diff --check` 仍报告开始前已有 `vision_actuator.c:68` 空白行尾随空格；未为清理它而触碰已有生产修改，本轮增量空白检查单独完成。
