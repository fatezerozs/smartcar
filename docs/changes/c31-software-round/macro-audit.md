# C31 启动策略与宏依赖审计（软件阶段；未验证）

基线 HEAD：`78f2d53`。此文件记录 2026-10-27 工作区内源码与构建脚本的定点文本审计结果；不是编译、测试或实车结论。审计范围限于 `demo/code`、`tools`、`tests/vision` 和已知的 Keil 工程，不访问 `.build/video-deps`。历史 patch/快照不作为活跃宏消费者。

## 构建目标矩阵

| build_midterm.ps1 入口 | AUTONOMOUS | ROUTE_TRIAL | ROUTE_SWITCH | AUTO_TRACK | 启动语义 |
|---|---:|---:|---:|---:|---|
| `train` | 0 | 0 | 0 | 默认 0 | 训练/诊断，不能假设自动启动 |
| `route` | 0 | 1 | 0 | 默认 0 | 路线实验，保留既有运行许可 |
| `switch` | 0 | 1 | 1 | 默认 0 | C26 电平许可，保持既有启动/恢复语义 |
| `auto-track` | 1 | 1 | 1 | 1 | C31 连续有效赛道 3000ms + 准入检查，不使用 C26/C27 |
| `auto` / `auto_track` | 映射到 auto-track | | | | 兼容别名，不再单独生成旧延时启动目标 |

宏缩写分别对应 `VISION_MIDTERM_AUTONOMOUS`、`VISION_MIDTERM_ROUTE_TRIAL`、`VISION_ROUTE_SWITCH` 和 `VISION_MIDTERM_AUTO_TRACK`。表内是脚本当前生成逻辑，尚未经编译验证。

## 宏审计与决策

| 名称 | 活跃源码或构建引用 | 决策 |
|---|---|---|
| `VISION_MIDTERM_AUTONOMOUS` | `midterm_boot.c`、`app_runtime.c`、`app_mode.c`、`vision_actuator.c`、`vision_motion.c`、`vision_probe.c`、`vision_startup.c`、`build_midterm.ps1`、多个主机目标 | **保留**：独立 autonomous 许可/诊断隔离边界 |
| `VISION_MIDTERM_ROUTE_TRIAL` | `vision_profile.h`、`vision_drive.c`、`app_runtime.c`、`midterm_route_profile.h`、route/switch/auto-track 构建与测试 | **保留**：C31 路线标定与算法构型 |
| `VISION_ROUTE_SWITCH` | 视觉控制、轮速、驱动、启动、运行时、遥测与 switch/auto-track 构建 | **保留**：算法构型不能被误当作开关启动许可 |
| `VISION_MIDTERM_C31_AUTONOMOUS` | `vision_profile.h` 派生表达式；`board_io.c`、`app_mode.c`、`midterm_boot.c`、`vision_drive.c` 等条件分支 | **保留**：派生模式，专门隔离 C31 auto-track |
| `VISION_MIDTERM_AUTO_TRACK` | `vision_profile.h` 编译期约束、`build_midterm.ps1`、`run_auto_track_tests.cmd` | **暂保留**：显式目标标记/兼容约束；功能上可能冗余，但不是无引用宏 |
| `VISION_START_SWITCH_ENABLE` | `vision_profile.h`、`vision_drive.c`、`midterm_profile.h`、`tools/verify_project.ps1`、多个旧测试构建参数 | **保留**：旧 C12 路径与编译约束仍使用；auto-track 构建显式指定 0 |
| `VISION_AUTO_START_ENABLE`、`VISION_AUTO_START_DELAY_MS` | `vision_profile.h`、`vision_drive.c`，旧 `vision_startup.c` 调度读取 profile 字段 | **保留**：兼容旧 profile/延时调度路径；auto-track 不使用该启动入口 |
| `MIDTERM_VISUAL_REVIEWED` | `midterm_profile.h` 默认 0；`midterm_boot.c` 实际启动准入；主机测试的正反向构型 | **保留且默认关闭**：相机轴校准和架空车轮方向检查未由软件自动宣告通过 |
| `app_mode_start_request` | 活跃 `demo/code`、`tools`、`tests/vision` 中已无定义/调用；历史 patch 中可保留证据 | **已移除**：无消费者的遗留事件接口，不能据此删除 C26 switch 的实际电平许可 |

## 软件保护与构建输出

1. `auto-track` 编译宏明确包含 `VISION_START_SWITCH_ENABLE=0` 和 `MIDTERM_VISUAL_REVIEWED=0`，不会因普通构建而自行声称已完成视觉审核。
2. `build_midterm.ps1` 拒绝基底 Keil 工程继承自主策略、开关启动、诊断、视觉审核等受控宏覆盖，避免同一宏互相冲突。
3. 构建脚本仅生成 `.build/midterm-<variant>/Objects`、`.build/midterm-<variant>/Listings`；如果原工程路径无法安全替换，则直接报错，避免意外写入发布目录。
4. `auto` 现作为 `auto-track` 的弃用别名；旧代码里仍有原始 autonomous 兼容分支，但正常构建入口不再选择旧固定延迟方案。
5. 以上条目均只经源码静态核对，**本轮未执行构建或测试**。旧入口对应的源码、硬件定时和工程目标仍需后续验证。

## 尚待验证/后续清理

- 审核当前所有 Keil 目标的完整预处理宏集合，确认没有目标外隐藏宏（仅源代码宏搜索不能代替编译时输出）。
- 构建 `train`、`route`、`switch`、`auto-track`，确认源文件集合与输出路径一致。
- 检查 `VISION_MIDTERM_AUTO_TRACK` 的兼容约束是否还需要显式目标标识；若未来改为统一配置枚举，再集中删除，不能只删除一处定义。
- 验证 switch 的 C26 电平语义、auto-track 的 C26/C27 完全不参与、UART arm/clear 不可用。
- 测试真实采集时间戳、软件 STOP 锁止与普通 MCU reset / 真正断电之间的区别；未验证前不声称已实现跨软件复位持久锁止。

没有发现足够证据支持删除上述仍有实际消费者的宏，因此本次**没有强行删除宏**。对新目标而言，正确隔离及拒绝危险组合优先于减少宏数量。
