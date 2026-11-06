# 当前硬件状态

本轮静止OFF及两组逻辑ON/重新授权遥测已完整取得，B3运动未完成。第二组出现左轮反向保护；不追加硬件授权请求。详见[本轮硬件收尾报告](HARDWARE_STATIC_REPORT.md)。下文为原软件准备状态与使用说明，NOT_RUN不代表本轮最新状态。

# B3：先完成主线静止前置验证

日期：2026-11-06。用户要求完成B3，随后明确选择“尚无实测数据，先完成静止验证”。**目前完成静止测量工程、Host验证和独立构建；尚未进行B3实板静止回归或运动Session，不能标B3完成。** 本轮未打开串口、未烧录/复位、未恢复动力。

## 可审核的具体候选

- 名称：RECORDER_B3_STATIC_C26_1（只用于静止前置测量，电机和舵机PWM驱动强制0）。
- AXF：C:/shengchanli/smartCar/smartcar/.build/recorder-b3-20261106/run-20261105T213819-31128-final/Objects/recorder_b3_static.axf。
- 对应Keil工程：同目录candidate.uvprojx。
- 源码工程：demo/recorder_b3/mdk/recorder_b3_static.uvprojx。
- 构建产物BIN/MAP/日志/inputs.json/build-evidence.json均在该新目录；旧C31/B2及其他AXF/BIN不覆盖。

Keil ARMCLANG6.21：0错误0警告。主线宏AUTO=0、ROUTE_SWITCH=1、ROUTE_TRIAL=1、ENCODER=1保持，复用C26主线算法和开关owner；MAP无midterm_boot_poll、Recorder训练Control、SD写入或RTOS任务。静止专用输出适配不是生产PWM等价证明。

## 下一步具体操作

保持底盘固定，电机和舵机供电隔离，C26置OFF（原程序读高电平1）；保持Camera/Encoder连接。由用户手动换烧上述AXF并Run后告知，再执行已授权的静止验证。该候选不需要恢复动力，编辑任何参考JSON也不会解除代码中的零占空比约束。

先执行C26 OFF的5秒基线：工具核对build指纹、outputs=forced_zero和C26 OFF，原生STOP/USB所有权申请后只采集RAM遥测。完成后只在OFF且控制停止时导出，主机保留原始64字节事件及CRC；不访问SD。之后分步骤安排人工C26 ON/OFF和手动转轮，获取缺少的实际覆盖，不能把OFF基线直接勾为全部前置通过。

## 交付导航

- [静止测试规程与阶段门](STATIC_TEST_PROTOCOL.md)
- [源码复用与隔离审计](STATIC_SOURCE_AUDIT.md)
- [B3T1静止遥测格式](B3T1_FORMAT.md)
- [参考几何与来源等级](REFERENCE_GEOMETRY.md)
- [参考几何JSON](REFERENCE_GEOMETRY_V0_1.json)
- [尚未配置的运动条件](MOTION_PROFILE_PENDING.json)
- [交付清单与哈希](DELIVERY_MANIFEST.json)
- [构建证据](evidence/static-build-evidence.json)
- [未提交改动保护复核](evidence/preservation-review.json)

## 验收状态

| 项目 | 当前结果 |
| --- | --- |
| B2 | 原静止Camera+SD短窗通过；结论保持 |
| C26/PI/Encoder原始Host | PASS：4项定向回归（含私有钩子版本），没有改断言 |
| 新记录器Host | PASS：实际C实现的有界缓冲、STOP、时间窗口、导出门、CRC、C26触发与等待超时 |
| PC解码 | PASS：8项，含损坏/短读/未知版本/非法输出/引用丢失/时钟回绕 |
| Keil/MAP/依赖 | PASS：0错0警告，425冻结输入，原工程不变 |
| Encoder/PI/C26 OFF实板 | NOT_RUN，待用户手动换烧 |
| 控制与SD并行 | NOT_RUN；本候选是无SD的主线静止基线 |
| B3运动Session | NOT_STARTED；缺上述实测及本车counts/轮、安全限幅等条件 |

本轮852项开发前基线中，原源码/工程变化0、缺失0；docs/INDEX.md有并行参考文档索引变化，已保留并单列记录，原未提交内容及主线项目保持原样；新源码与所有适配只在独立目录/新输出路径中。没有修改ACL、Git历史、全局代理配置或技能。
