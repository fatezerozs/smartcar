# 内存与 DMA 预算

所有容量分为 MAP 静态证据与实测。没有将链接地址当作实板测试，也没有假定存在任意1MiB连续片内RAM。

| 对象 | 静态大小/配置 | 证据与限制 |
| --- | ---: | --- |
| 原灰度 payload | 188×120=22560B | 源码配置，实际相机规格 UNKNOWN |
| CSI raw | 8×22592=180736B | 64字节对齐；2 DMA占用、其余最多6个暂存；ownership prepare→换地址→DSB→publish |
| Writer pool | 64×22656=1449984B | 固定记录池，含每帧64B元信息/对齐；filled/free队列各64个单字节索引 |
| vision 副本 | 3×22656=67968B | 复用 Capture 模块仍保留其 copy/pending；B2 不创建 Vision 任务、不运行视觉算法 |
| SD bounce | 32768B | 独立非缓存，实际每批最多64扇区=32KiB；读写从不借用 raw/queue 地址 |
| SD card / descriptors | 772B / 128B | MAP，64/4字节对齐 |
| SDRAM_ncache | used1664448 / max2097152B，剩432704B | 既有 Recorder MAP 与新B2候选相同容量；地址见JSON；不是实测硬件可达性 |
| C26 DTCM区域 | used186620 / max449536B | 本轮主线MAP；heap1024、MSP8192另列，不能重复当空闲 |
| 训练 Recorder DTCM | used197496 / max449536B | 已有MAP；5任务堆栈总54272B，含Idle；不是当前纯Camera任务预算 |
| B2 DTCM区域 | used83788 / max449536B | 新候选MAP；3任务+Idle的栈25600B，另MSP8192/库heap1024 |
| B2 SDRAM缓存执行区 | used116864B | 代码/只读/可缓存对象合计；不要把它称作全是代码或全是RAM帧 |
| RTOS heap | 动态分配关闭 | configSUPPORT_DYNAMIC_ALLOCATION=0；所有任务/队列静态分配，运行栈高水位仍 UNKNOWN |

B2 关键地址：raw=0x81e00080；recording=0x81e2c280；bounce=0x81f8e5c0；g_sd=0x81f8e280；descriptor=0x81e00000，均落在[0x81e00000,0x82000000)。链接脚本、DCD 与 BOARD_ConfigMPU 宣告该 SDRAM布局及末2MiB非缓存；实际板型、DMA读回/cache一致性仍待实板验证。

## 带宽与停顿预算

以配置50FPS计算，图像payload=1128000B/s，SCRAW帧头尾44B后=1130200B/s（约1.078MiB/s），还需分段metadata/GAP/END/文件系统开销。该速率仅是静态负载预算，不是测得帧率。
公式：B_required >= R_peak*T_stall + B_burst + B_metadata + B_margin。取旧观测1.040451s只作压力假设，不能当保证上界；64个slot在50FPS仅覆盖约1.28s，Writer持有一个时可排队量更少。更长停顿、CPU CRC、双重复制、SD中断、CSI仲裁都可能使队列溢出。
B1按用户跳过，没有长期R_effective/队列斜率和更长尾样本；不得据此判定持续完整记录能力。B2候选只用于短窗，不能替代10–30分钟B1。

CSI IRQ优先级2，高于FreeRTOS syscall阈值5；该IRQ只处理ownership/地址，不能调用RTOS阻塞接口。USDHC IRQ5可向OSA事件发信号；UART5相机配置IRQ6。Capture任务4，Writer2，B2 Monitor5。已检查向量绑定到port.o；中断屏蔽时长、复制WCET、任务高水位与CSI/SD实际并发均 UNKNOWN。
完整MAP摘要在evidence/recorder-map-summary.json、primary-map-summary.json、b2-build-evidence.json。

## B2 metrics=2 最终预算与实测更新

当前最终MAP：run-20261105T202447-47412-final。DTCM RW_m_data使用95020/449536 B；非缓存帧/SD区原预算1664448/2097152 B仍适用。采集raw、记录pool、SD bounce与描述符位置和对齐经原MAP校验工具复核；未链接车辆业务。新增带哨兵的固定512样本统计数组计入初始化数据；没有递归qsort。

本轮实际栈最小余量：Capture996、Writer3540、Monitor868、Idle246 words；排序/打印后Monitor余量846 words。Capture复制路径最大2217us，队列高水位33/64。所有这些仅对应本次101帧窗口，不是所有输入下栈或DMA/cache一致性的形式化保证；全量PC CRC/图像重建未发现本次可见一致性异常。详见B2_CAMERA_SESSION_VALIDATION.md。
