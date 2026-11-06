# B3前置静止验证规程

## 已有授权与硬件状态

用户要求B3，确认封闭场地和人工急停已准备，随后选择“尚无实测数据，先完成静止验证”。本规程先执行该静止范围，不恢复动力。B2通过事实保持，但主线C26的Encoder/PI/输出链和SD并行不能由B2替代。

换烧前：底盘固定，电机与舵机供电隔离，Camera/Encoder连接；C26 OFF（程序GPIO高1），不要执行转向/电机命令。用户手动烧录当前candidate AXF并Run，再由代理核对身份。软件强制0不能代替实际供电隔离，尤其不能把原生产工程误当此静止候选。

## S0：C26 OFF基线

工具 tools/test_recorder_b3_static.ps1 默认COM14，115200、DTR/RTS=false，无复位/烧录功能。

1. 原生!停止、x申请USB所有权；核对RECORDER_B3_STATIC_C26_1、outputs=forced_zero、正确输入SHA、C26 OFF/OBSERVE。
2. 发送@b3 capture，记录一次5秒RAM窗口；不发启动/电机/舵机指令，不访问SD。
3. 等待FROZEN；只有OFF稳定且非RUNNING才允许@b3 dump。
4. 读取最多196608 B原始事件，严格校验版本/长度/CRC；唯一新主机目录保留原始、header、日志、CSV和报告，关闭COM14。
5. 判读真实采样间隔、原始计数、PI服务gap/mask、视觉时间与三个PWM寄存器快照。静止0counts本身不能证明Encoder会计数；OFF状态的inactive PI不能当active PI通过。

## S1：C26 ON→OFF逻辑路径（仍断电固定）

确认S0通过后，使用-C26TransitionTest。工具只发送@b3 arm等待物理C26，不用UART授权运动；按前台轮询的30秒软件等待上限（非硬实时保证）未触发则为空超时，不伪造测试数据。工具提示ARMED后，由人工拨ON，再在5秒记录窗口内拨回OFF；输出仍被驱动适配强制0。初始原始ON被采样即触发RAM；不在ISR打印或启动电机。

检查原生ON授权条件、实际active PI是否被覆盖、OFF后的请求归零与故障处理、重走OFF→ON的重新授权语义。静止无Encoder运动可能触发原生no_count故障，需保留具体原因；这不是已测真实车速或实际闭环跟踪。首采样OFF到零请求的时差不同于物理触点边沿延迟；全程applied=0不能证明实际非零PWM的断电停车距离。

如果结束时仍ON，工具拒绝dump，RAM保留；人工OFF后用-DownloadOnly回收同一trace，不重启、不覆盖。空的等待超时可clear_empty；非空未导出的trace禁止新CAPTURE。

## S2：Encoder静止手动检查

维持供电隔离并支撑底盘，由人工分别缓慢转动单侧轮，记录RAW和SAMPLE通道对应、方向、连续性及计数变化。只有人工明确记录整圈数、方向、左右轮及重复测量条件后，才能形成counts/轮；不能把raw3/raw4（软件按10ms归一化）当真实原始累积counts。

目前用户未提供counts/轮或负载周长；该阶段需要人工配合，不能由脚本自行转轮或填补数据。参考直径/轮距只用于Host，不产生m/s限速。

## S3及B3真实运动门

无SD基线实测之后才制作同控制owner的Camera+SD静止对照，验证Encoder/PI/最终输出deadline、IRQ/DMA争用、SD失败不反向影响控制，并实现正式运动Session的原始SCRAW与版本化遥测sidecar配对。当前B3T1仅为静止RAM诊断，不声称已实现这些项。

B3运动前必须有实测counts/轮、方向、滚动尺度、舵机限位和安全PWM/速度边界，冻结具体限速、有限时长/距离、C26 OFF和人工急停条件。缺数值时保持MOTION_PROFILE_PENDING未配置，不沿用旧经验高速参数当新试验限速。用户确认场地准备不免除这些前置实测；后续仍按具体试验方案确认恢复动力/运动。本轮没有运动Session，因此B3不计完成。

## 判定与证据

CRC/长度/索引/时钟错误、缓冲满或缺引用必须保留原始并标invalid/incomplete；不自动无限重试。pc_verified只表示trace传输/结构验证，不能直接给C26/PI实板或B3运动打PASS。报告中的b3_motion_accepted与production_ready始终false；人工和板端实际覆盖逐项记录。

实板ON覆盖还要求相机看到有效赛道，且记录到实际consumed递增；若只看到等待/无有效视觉，不能把它当ON启动或active PI已通过。
