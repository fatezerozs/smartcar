# B2 Session 只读导出交付与下一步

## 当前状态

真实 Camera 短窗已于 2026-11-06 完成：2:/R0000001，板端 observed/written/synced=100，error/drop/pool/ambiguous=0，queue 高水位32。完整 Session 尚未下载，B2 完整验收待定。

用户要求继续完成 B2 后，在原 B2 工程加入只读目录导出，没有创建另一种独立固件。旧相机/Writer和SCRAW格式不变，原生产工程不变。本次开发阶段未访问串口或重新采集。

## 待手动换烧的具体候选

C:/shengchanli/smartCar/smartcar/.build/recorder-camera-stages-20261106/run-20261105T195937-29944-final/

- Objects/recorder_b2.axf：待烧录。
- candidate.uvprojx：对应归档 Keil 工程。
- Objects/recorder_b2.bin、Listings/recorder_b2.map、keil-build.log、inputs.json、build-evidence.json：构建与隔离证据。

Keil 0错误0警告，MAP无执行器/Encoder/Control业务符号，192依赖冻结与最终.d一致。文件哈希见 evidence/b2-export-build-evidence.json。旧AXF/BIN/MAP保留。

## 使用顺序

用户手动换烧并 Run 后告知即可；不要自行发送 B2 START，不需重新采集，也不要删除卡内 R0000001。换烧不需要格式化或修改 SD 分区。

主机工具 tools/download_b2_session.ps1 默认 COM14、R0000001：

1. B2 STATUS 验证 idle 固件身份与 export=1。
2. B2 EXPORT MODE 启动仅 Monitor 任务的只读 SD 模式，不初始化相机、不创建 Capture/Writer 任务。
3. 升至921600、LIST目录、逐块 READ，块长最多4096字节；核对目录/文件/偏移/长度/CRC32，传输CRC失败最多3次，其他错误停止。
4. 全部文件保存在仓库外的唯一 b2_real_session_UTC 目录；现有主机文件不覆盖，未完成下载保留.partial。
5. 正常完成恢复115200并关闭串口；异常时尽力恢复并记录，不复位、不自动START。
6. 自动执行 verify_b2_session.py：目录与manifest一致性、SCRAW头/metadata/record/payload CRC、序号/END边界/时间戳、逐帧原始字节与PNG还原、原像素统计、缺帧清单、全量timeline、Label Studio抽样与源文件SHA256。

固件也能在当前启动的Writer已关闭后导出，但主机自动工具故意要求冷启动idle→只读模式，以避免误启动或活动Writer竞争。范围只允许 R+7位大写十六进制目录、session.json、6位数字.scraw。READ只用FA_READ，不具备删除、覆盖、格式化、扩展文件等接口。超时/磁盘错误沿用SD DMA永久隔离，禁止继续复用潜在未完成DMA缓冲。

## 验证与限制

- Host C导出9场景通过：非法路径/范围、目录列表、意外目录项、成功读与CRC、短读、挂载失败、超时、隔离状态、关闭失败。
- PC包装器3测试通过：全像素/PNG往返且synthetic不能冒充实板；额外文件拒绝；CRC损坏必须incomplete。
- C#客户端CRC已知向量、文件名与全部数据头字段变异拒绝自检通过，CheckOnly不打开COM。
- 实板导出功能尚待用户手动换烧后执行；不能用Host结果认定文件已读回。
- 全部100帧的实际间隔/有效记录帧率、像素内容、完整性仍待下载。配置188×120/GRAY8/50FPS与实际传感器规格必须分开。旧Session没有实际曝光读回；无Encoder/控制遥测。
- CSI/SD并行短窗已有板端队列与错误计数，精确CPU负荷、Writer逐调用延迟、IRQ/DMA争用分布仍无实测；下载后逐条裁决B2要求，不会仅因CRC正确就全部勾PASS。B1仍为用户跳过，B3/C1/D/E不放行。

读取层：Markdown/代码原生文本及XML/JSON为structured_parsed；Host、Keil、MAP、CRC为computed；没有md_extracted或pdf_verified证据。
