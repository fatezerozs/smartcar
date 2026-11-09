# 无线通信模块交付 — 2026-11-08

状态：软件实现、定向回归和独立构建完成；等待换烧后模块实板验收，不计运动通过。

## 本模块范围

无线报告改为RMR1：4字节magic、uint32小端序号、uint16小端载荷长度（1—96）、载荷、uint32小端IEEE CRC32。CRC覆盖magic/序号/长度/载荷。单帧最多110字节；printf长行拆为连续短帧，原始接收字节完整保留。

Python解码器跳过非RMR1噪声，CRC不符则重新同步；首次连接或帧序号跳变时丢弃到下一个换行，不把不连续碎片拼成有效报告。积累文本最多4096字节；无magic噪声缓冲最多3字节。

新MOTION PING <16位十六进制随机挑战>只回复PONG，包含挑战、source和C26。20秒板端测试只发送PING/STATUS，5次有界查询；至少一条完整的匹配指纹、OFF、Camera/Storage未启动身份，且至少两条本次挑战应答，才计模块PASS。CRC与随机挑战检测错误及旧应答，不保证无线可用率，不会修复丢失内容。

## 交付与测试

当前工程：C:/shengchanli/smartCar/smartcar/.build/recorder-joint-20261106/motion-radio-20261108T135742607278Z/candidate.uvprojx。
AXF：同目录Objects/recorder_motion_c26_3s.axf。
输入摘要：9312b356d53025ebbfde778b4c74877315aa06950e86e34ec2d2fba5a8f047e2。

- 3项Host测试PASS：真实C生产帧→独立Python CRC解码；队列反压/固定空间/发送超时；逐字节分片、前缀噪声、损坏帧、截断、序号缺口、过期nonce/来源/OFF门；10万字节噪声后缓冲有界。
- 实际motion_guard启动前外设访问、限幅、截止、OFF与回绕测试PASS。
- Keil两轮构建0错误0警告，输入哈希/AXF哈希及MAP内存与保护符号检查PASS。
- 单元测试入口：python -m unittest discover -s tests/recorder -p test_motion_radio.py -v。
- 板端模块测试入口：python tools/check_motion_radio.py --port COM10 --build <上述目录>。只在人工换烧并Run、C26 OFF和动力断开后执行。

原MotionClient.cs按裸ASCII行读取，不能直接消费RMR1报告；本模块先用check_motion_radio.py验收。运动流程客户端接入RMR1及板端运动属于下一模块，不能运行旧客户端凑通过。SD二进制文件回读仍沿用旧原始payload，仅报告文本被封装；完整运动回读接口尚需集成，不以本模块宣称完成。

## 自审

仅变更实验生成器和通信测试/解码工具；原生产控制源码、视觉门、2.9秒输出许可、20%电机PWM上限未更改。保留原始失败数据，无本轮硬件发送/运动。输入计数与实际镜像哈希保存在候选motion-evidence.json。

证据层：C/JSON/XML为structured_parsed；Host/CRC/哈希/构建为computed；未使用md_extracted/pdf_verified。未取得新版无线板端结果，混入数据的真实来源仍未确证。
