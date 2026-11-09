# main原生无线模块迁移

2026-11-08。软件迁移与构建完成，板端验收待做。用户要求直接复用main，旧自定义RMR1/nonce方案停止推进，原型文件及结果仅作历史保留，不链接到当前候选。

来源为已发布origin/main=f7fb5b9，与当前HEAD一致。本机main名称仍停在de14157，未切换分支或回退代码。app_transport.c/h、app_link.c/h、app_link_board.c按统一换行逐字节比对一致；候选直接链接原文件，不改主线实现。

启动复用app_transport_init，接收复用app_transport_begin/read/stopped/changed/poll，报告复用app_console_printf；继承原生owner/STOP/错误丢弃/RTS/队列。实验命令增加@前缀（@MOTION STATUS/START/ARM/STOP），使用原生行模式，防止STATUS/START的S触发单字符STOP。只在候选命令适配剥掉@交给原有限时状态机。控制与2.9秒/20%PWM限制不变。

当前工程：C:/shengchanli/smartCar/smartcar/.build/recorder-joint-20261106/motion-radio-20261108T140141769996Z/candidate.uvprojx。
AXF：同目录Objects/recorder_motion_c26_3s.axf。
source：c20c8126df7cbb52490432146b04a0f4505283e021888a718bb8affdc5e38c0b。

验证：既有transport、link-board与环境检查全PASS；运动guard实际C启动前外设访问、截止/限幅/OFF/回绕通过；Keil两轮0错误0警告，输入/AXF哈希与MAP检查通过；MotionClient增加@前缀后编译自检通过。

换烧并Run，保持C26 OFF、动力断开。模块验收入口：python tools/check_motion_radio.py --port COM10 --build <上述目录>。20秒最多5次@MOTION STATUS，只保存原始数据并检查至少两条完整匹配source/OFF/Camera0/Storage0身份，不发送START/ARM。通过后才进入运动模块，使用带@前缀的现有MotionClient。

限制：复用main不等于当前无线干扰自动消失；ASCII协议未增加CRC，不能恢复截断字节。SD文件串口回读仍需单独验证，非阻塞队列满可能拒绝发送，不把通信软件通过等同于文件回读或运动验收。

读取层：C/XML/JSON为structured_parsed；来源比对/Host/构建/哈希为computed；无md_extracted/pdf_verified。无本轮板端或运动通过结论。原始失败资料保留。
