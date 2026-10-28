# C31 软件架构改造状态（未验证）

> 2026-10-27 后续软件验收结果见 [验收交接](acceptance-2026-10-27.md)：switch 等主机回归与五次 Keil 纯构建已通过，预处理 equivalence 第 4 帧仍失败。下文保留原实现阶段的历史状态，不代表当前所有项目仍未执行验证。

## 当前工作区基线

- 基准 HEAD：`78f2d53`
- 本文记录的是工作区当前源码状态，不代表已合并提交。
- 本轮按要求只推进软件实现；没有执行测试、编译、lint、格式化、Keil/UV4、烧录或实车操作。
- 现有未提交修改及未跟踪文件必须保留，尤其是协议工具兼容性修改、`tests/vision/test_auto_track.c`、`tests/vision/run_auto_track_tests.cmd` 和历史快照目录。

## 已具备的软件边界（仅源码检查）

### 帧观测

- `vision_frame_stamp_t` 携带采集序号、采集时间和时间有效标志。
- `vision_frame_copy()` 对复制前后的完成标志、缓冲区指针和序号进行一致性检查。
- `app_runtime.c` 将采集序号/时间传入视觉跟踪和驾驶观测；不可信时间戳不能作为有效驾驶观测。
- 软件接线已完成：受 Git 跟踪的 `libraries/zf_device/zf_device_mt9v03x.c/.h` 已添加采集元数据；`mt9v03x_finished_callback()` 在有效双缓冲完成分支采样 `board_camera` 注入的 GPT 时钟、递增序号、设置 `time_valid`，最后设置 `finish_flag`。真实板级时钟、中断完成时机、DMA 缓冲区所有权和时间回绕语义仍待后续验证，尚未通过板级验证。

### 自动启动

- C31 auto-track 使用独立派生构建策略：`VISION_MIDTERM_AUTONOMOUS && VISION_ROUTE_SWITCH && VISION_MIDTERM_ROUTE_TRIAL`。
- 启动等待要求新鲜、有效、不同序号的视觉观测连续覆盖 3000ms，然后进入准入检查。
- `MIDTERM_VISUAL_REVIEWED` 默认仍为 0；未明确审核的构建必须保持锁止。
- auto-track 不应采样 C26/C27，也不应通过旧开关路径清错或重启；switch 目标仍保留 C26 许可语义。
- 关键故障/STOP 后的软件锁止与“必须真实断电重启”不是同一件事。若板级硬件不能可靠区分 power-on reset 与普通 MCU reset，不能声称已经实现跨软件复位的持久锁止。

### 执行器输出

- `vision_actuator.c` 集中处理正常驱动、运动实验和探针的输出提交；授权与故障状态在提交前检查。
- C31 auto-track 的 C26/C27 interlock 已在编译期隔离；`switch_sample` 在该构型为 no-op；auto-track 的 probe/motion 入口拒绝。
- 前台输出路径在帧超时边界先锁止并记录 `VC_FAULT_FRAME`，避免与 PIT fault 竞争导致状态机仍显示 RUNNING。
- 每次归零撤销软件输出授权并清除输出缓存。
- 仍需通过后续代码审查与测试确认所有 PWM 写入点均已迁移、PIT/前台临界区无竞态，以及底层 PWM 缓冲/LDOK 的实际生效时序。PWM=0 不代表有已知停车距离。

### 轮速控制

- 目标发布和 PI 计算已分为 `vd_speed_output()` 与 `vd_speed_control_poll()`。
- PI 仅消费新编码器样本；重复消费、样本积压、采样间隔过大、控制服务延迟和过期样本应 fail-closed。
- 当前控制服务仍由前台运行时调用，不是经硬件验证的硬实时固定周期调度器。不要把这一点描述为固定周期控制已完成。
- 目标 ticks、实际 PWM 上限、PI 积分限幅和目标变化率是不同概念。后续必须继续保持配置和日志语义分离，不能借架构整理改变已确认的编码器符号、PI 增益或路线目标。

## 构建策略与宏审计状态

- `tools/build_midterm.ps1 -Variant auto` 现在作为兼容别名映射到 `auto-track`，并给出弃用警告，避免常规构建入口继续生成旧的固定延时启动目标。
- `VISION_MIDTERM_AUTO_TRACK` 暂时保留为显式目标标记/编译期约束；它并非无引用宏。
- `VISION_AUTO_START_ENABLE`、`VISION_AUTO_START_DELAY_MS` 暂时保留，因为旧启动调度接口仍被非 auto-track 构型使用；auto-track 不应依赖这条路径。
- `VISION_START_SWITCH_ENABLE` 暂时保留，因为 legacy C12 路径、编译保护和既有主机测试仍引用它；auto-track 构型必须将该功能关闭。
- `VISION_ROUTE_SWITCH` 仍是 switch/route 算法构型的一部分，不应作为启动许可本身。auto-track 的板级模式输入路径应继续保持隔离。
- 本轮没有删除上述宏，因为现有源码/构建/测试中仍有有效引用。后续只应删除经全仓定义/引用审计确认无用途的宏。

## 后续软件工作

1. 定点审计每个 PWM 写入点，确认普通非零输出只通过统一仲裁入口。
2. 全局输出时序/ISR 竞态仍待后续验证；继续审查前台/ISR 共享状态、授权撤销、缓存 PWM 和输出复活竞态。
3. 已将轮速采样/控制的名义间隔、最大采样间隔、样本新鲜度、控制服务超时、启动采样新鲜度和未消费样本上限集中命名在 `vision_speed.h`；当前仍是前台服务，未宣称为硬实时固定周期。
4. 使用 `git grep` 对启动、模式、诊断、C26/C27、C12、auto-track 等宏做定义/引用审计。仅删除能够证明无消费者且没有外部构建意义的宏；疑义宏保留。
5. 统一 train/auto/route/switch/auto-track 的宏配置和 Keil 工程输出目录，所有构建产物写入 `.build`，不覆盖 `releases`。
6. 更新接口注释、启动策略说明和诊断状态，使它们与当前实现一致。

## 后续验证顺序（本轮未执行）

新增 auto-track 测试源码未运行，不能据此声称验证通过。

1. 只读审查完整 Git diff，确认无覆盖既有工作区修改。
2. 编译各构建宏组合，尤其 train、auto、route、switch、auto-track。
3. 执行帧新鲜度、启动状态机、执行器仲裁和故障锁止主机测试。
4. 执行现有 switch/route 回归与协议测试。
5. 检查输出目录和发布文件校验和。
6. 最后再制定独立的硬件验证计划：采集时间戳、PWM/LDOK 时序、真实断电复位语义、控制周期和实车安全停止。

本文件是实现状态与待办清单，不是验证报告；所有测试/构建结果仍待后续执行。
