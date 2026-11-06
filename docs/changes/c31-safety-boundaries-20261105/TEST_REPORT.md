# Phase 0 基线检查与 Phase 2/3 测试计划

日期：2026-11-05。状态：整改前的选择性基线，不是完整回归，不是整改验收。

## 已执行命令

命令均从 `C:\shengchanli\smartCar\smartcar` 执行。编译器发现和测试只使用现有本地工具，没有下载或升级。

| 命令 | 退出码 | 结果 | 日志 |
|---|---:|---|---|
| `pwsh -NoLogo -NoProfile -NonInteractive -File tools/testing/Invoke-TestPlan.ps1 -Suite recorder-startup,recorder,recorder-tools -Compiler msvc -Mode optimized` | 1 | 2 PASS / 7 BLOCKED / 0 FAIL / 0 SKIPPED | `.test-results/20261105T130849-f172cf759d/summary.md`、同目录 run.json |
| `pwsh -NoLogo -NoProfile -NonInteractive -File tools/testing/Invoke-TestPlan.ps1 -Suite recorder-startup,recorder,recorder-tools -Compiler gcc -Mode optimized` | 0 | 9 PASS / 0 BLOCKED / 0 FAIL / 0 SKIPPED | `.test-results/20261105T130958-69d4e9fa0f/summary.md`、同目录 run.json |
| ARMCLANG 6.21：4 个生产单元 `-E -dM` | 全部 0 | 4 个预处理导出成功；3 个单元各有 1 个 include 大小写 warning | `.build/c31-safety-20261105-phase01/analysis-commands.json` 及各单元 macros/log |
| fromelf 对既有 recorder_train.axf 执行 `--text -c --output ...` | 0 | 既有二进制反汇编导出成功，未重新链接 | 同目录 analysis-commands.json、fromelf.log、baseline-disassembly.txt |

9 个 GCC Suite 为 environment、compiler-environment、vision、control、speed、link-board、recorder-startup、recorder、recorder-tools。这包括依赖，不是 9 个新增 recorder 测试。recorder 为 17 个 Python 测试，recorder-tools 为 3 个生成契约测试；startup 有 review=0 和 review=1 两构型，后者执行 22 个场景。其输出的 3006/179034 checks 是断言计数，不能冒充独立测试用例数。

MSVC 环境检查未找到编译工具：PATH 中没有 cl.exe，标准 Visual Studio 2022/2019 目录中也没有找到 cl.exe、vcvars64.bat 或 vcvarsall.bat。environment 与 recorder-tools 仍通过，其余 C Suite 因依赖编译器阻断。GCC 14.2.0 位于 `C:\Program Files\mingw64\bin\gcc.exe`，它是补充证据，不替代项目要求的 MSVC optimized/ASan。

生成契约测试在测试自己的隔离副本中运行 prepare；没有在生产 recorder 目录重新生成工程或覆盖构建。后续逐项哈希复核确保原快照文件未被改变。

未执行：完整 Host、PreFlash、ASan、Switch 全套、Link/Equivalence 全套、Keil 新构建、整改后的任何测试，以及任何硬件验证。上述通过只证明现有行为基线，不能证明 near-only 或 partial 的新安全要求已经满足。

## recorder 构型一致性计划

沿用 catalog 和现有 recorder-startup harness，扩展实际 recorder 配置变体；不新建平行框架。

- 使用生成后的 recorder `vision_drive.c` 或可验证的同一生成字节，检查其源码 SHA、40 ticks 配置、C31 宏、review 宏和关键 include 优先级。
- Host mock 与生产 IRQ/RTOS/PWM 的差异逐项记录；仅对硬件适配层做 stub，不替换被测控制/锁止/PI 生产逻辑。
- 对照生产编译单元的预处理结果与 Host 有效宏值；AUTO_TRACK 兼容断言宏的写法差异不能掩盖实际速度源不同。
- 原 generic auto-track、route-switch、bench、diagnostic 继续回归。审查来源锁对应 diff，再更新生成契约，不能禁用锁校验。

## 视觉矩阵：零动力窗口预期（参数待确认）

用户已选择保留零动力恢复窗口。以下 21 项均为待实现/待执行，不能把整改前的基线通过当作这些预期已通过。300 ms / 5 帧仅是讨论候选。

| 编号 | 输入 | 新设计应验证 |
|---|---|---|
| V-01 | 新鲜近远场合格 | 完整准入后正常输出 |
| V-02 | 单帧远场失效、近场有效 | 立即目标/PWM 双零，进入固定总等待窗口 |
| V-03 | 持续远场失效 | 达总期限后终止锁止 |
| V-04 | 短失效后连续恢复 | 截止前完整新帧达到门槛，平滑恢复原租约 |
| V-05 | 重复恢复序号 | 不计连续新证据，不续期 |
| V-06 | 过期恢复帧 | 不解除电机抑制 |
| V-07 | 近场失效、远场有效 | 不凭远场驱动，双轮零输出 |
| V-08 | 近远场均失效 | 双零等待有界，不沿旧路径驱动 |
| V-09 | 当前单左边界 | 近场未合格时最终双轮 PWM 为零 |
| V-10 | 当前单右边界 | 与左侧对称，不授权 partial 动力 |
| V-11 | 断续边缘/阴影/标记线 | 弱证据不刷新总起点或恢复计数 |
| V-12 | 交叉口/圆环/低对比度 | 完整当前证据合格才计恢复，失败保持零动力 |
| V-13 | 间歇 partial | 不无限刷新总期限 |
| V-14 | 时钟回绕/倒退/同毫秒帧 | 区分合法回绕，不产生错误 dt 或资格 |
| V-15 | frame timeout / gap | 独立于窗口终止，不等下一图像 |
| V-16 | runtime / encoder / numeric fault | 已启用保护终止，生产 runtime 禁用状态单列 |
| V-17 | 进入等待后无新帧 | 周期轮询/帧超时仍撤销授权 |
| V-18 | far loss / no lane / 单帧完整交替 | 总起点不重置，连续恢复中断清计数 |
| V-19 | 到期边界与恢复同轮 | elapsed >= limit 时终止优先 |
| V-20 | 积压/未来时间/乱序/序号回绕 | 不用处理延迟扩大租约 |
| V-21 | strong partial / junction_partial | 未通过近场资格时最终双轮 PWM 为零 |

scenario 19 的 near-only 输入保留，预期改为零动力等待并最终超时锁止。短时黑图与 200 ms 恢复输入保留，补连续恢复门槛及总期限；原瞬间恢复预期按确认后的政策更新，不删除场景。新增等待前非零 PWM、PI 清零、恢复时旧候选拒绝、速度/PWM 上升约束的端到端断言。

## STOP / 锁止矩阵

| 编号 | 输入/交错 | 必须验证 |
|---|---|---|
| S-01 | 正常运行 STOP | 立即撤销并双轮零输出 |
| S-02 | 远场降级等待期间 STOP | 立即终止，后续恢复帧不能复活 |
| S-03 | 无道路恢复等待期间 STOP | 立即终止，后续恢复帧不能复活 |
| S-04 | 新帧与 STOP 同轮 | STOP 优先 |
| S-05 | PI 准备/提交间 STOP | 旧 PI/输出候选被拒绝 |
| S-06 | STOP 后连续有效图像 | 不重启 |
| S-07 | STOP 后 heartbeat | 不续租 |
| S-08 | UART/Radio arm/clear | C31 拒绝旧启动/清故障权限 |
| S-09 | 故障后图像恢复 | 不自动清故障或 arm |
| S-10 | export | Control 抑制 ack 在 Storage 收尾之前 |
| S-11 | 运行中链路错误 | 保留终止语义 |
| S-12 | 显式模拟新 boot | 仅完整新 boot 准入可重新启动 |
| S-13 | 完成计算后故障/STOP 才提交 | 版本复核拒绝旧候选，检查 PWM 写入桩 |
| S-14 | 候选后配置/输出限制变化 | 新边界生效后旧高输出无效 |
| S-15 | STOP 与 FAULT 同轮 | 运行授权撤销，故障与原因均不丢 |

新增交错钩子只在 Host 构型存在。断言覆盖最终 `pwm_set_duty` 写入桩和输出缓存，不只检查 controller duty。PI zero、积分重置、样本只消费一次、版本回绕前提与进入前中断屏蔽状态恢复分别验证。

## P2 与全量回归计划

事件测试覆盖：任意初态开录、FIFO 满、终止槽合并、序号空洞、SD 阻塞/失败、会话关闭/STOP 交错、末水位排空、截断与缺 manifest、旧 session 无 sidecar、构建 ID 无匹配/多匹配。检查 Control 全程零等待，原始图像 bytes 与 SCRAW parser 保持兼容，Storage 仍是唯一 FatFs 写入者。

设计确认并实现后运行任务书第 6.6 节所有适用项目：新矩阵、实际 recorder Host、Speed/Link/Equivalence、Switch/Midterm、`tests/vision/run_switch_tests.cmd`、完整 Host/PreFlash、review=0/1 的 Keil recorder 纯构建及宏编译。每次保存真实命令、退出码、Suite/用例统计和日志。

缺失 MSVC 的项保持 BLOCKED，不用 GCC 或 ARM 预处理改写为通过。不安装工具、不降低断言。最终硬件实时性、SD 并发吞吐、PWM reload 和物理停车均单列待测。
