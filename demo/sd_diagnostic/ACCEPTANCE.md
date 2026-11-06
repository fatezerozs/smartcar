# SD Diagnostic 验收报告

日期：2026-11-06。结论：**独立工程构建与隔离证据交付完成，等待人工批准烧录。SD 硬件读写未验收。**

| 验收项目 | 结果与证据 |
| --- | --- |
| 源码审计 | 完成。`evidence/source-audit.md` 记录实际调用链、1-bit/3.3V 策略及原 SDK 轮询限制 |
| 独立工程 | `mdk/sd_diagnostic.uvprojx`，独立源码/启动/scatter/输出；与仓库 SDK 相对路径关联 |
| Keil 构建 | ARMCLANG 6.21，最终完整 build.ps1 退出码 0，0 Error(s), 0 Warning(s) |
| MAP 检查 | 16 个允许对象严格匹配；没有车辆、视觉、Recorder、RTOS 业务对象；无已链接 f_mkfs/f_unlink；DMA 对象 64 字节对齐且在 OCRAM |
| 主机模拟测试 | 15 项全部通过，含挂载失败、短写/短读、CRC 错误、同步/关闭错误、容量与名称冲突 |
| COM 端口枚举 | 注册表只读查询成功，WMI 被拒绝后未提权；COM10 无线、COM14 有线由用户确认；未打开端口 |
| 实际 SD 初始化 | 未执行，待批准后板端验证 |
| 实际文件写入 | 未执行，待批准后板端验证 |
| 实际回读 CRC | 未执行，待批准后板端验证 |
| 连续写入性能 | 未执行，待批准后板端验证 |

最终构建路径：`artifacts/run-20261106T001438910-29796`。

- AXF：`Objects/sd_diagnostic.axf`
- BIN：`Objects/sd_diagnostic.bin`（基地址 0x70000000，36032 字节）
- MAP：`Listings/sd_diagnostic.map`
- 日志：`keil-build.log`
- 哈希/输入依赖/内存/隔离与保护校验：`verification.json`
- Code 24112 字节、RO 4200 字节、RW 16 字节、ZI 63664 字节（包含保留栈/堆）；初始 SP 0x20280000，Reset 向量 0x70002415。

## 原工程与工作区保护

开始时在正确 Git 根目录记录 `evidence/git-status-before.txt`、`head-before.txt`、`baseline-sha256.json`。上层工作目录本身不是 Git 仓库，该只读探测没有改动文件。
对 Git 跟踪文件及明确允许的 Recorder/code/mdk/docs 范围内文件建立 964 项基线，最终逐字节哈希复核 **0 项变化**。Git HEAD 保持 `de1415743b004160f520d385c9e3e63008f52007`；C31、Recorder、原 Keil 工程及其现有未提交内容保持不变。本次写入仅发生在新建 `demo/sd_diagnostic/` 内。
未扫描 `.build/`、video-deps 或受限临时目录；未改 ACL，未扩大访问权限，未执行 clean/reset/checkout/revert，未覆盖原有 AXF/HEX/BIN。各次新构建分配不同输出目录。
最终 Git 状态另见 `evidence/git-status-final.txt` 与 `evidence/preservation-final.json`。

## 证据层级与风险

- `structured_parsed`：Keil XML 配置、JSON 哈希与基线、串口注册表枚举。COM10/COM14 用途为用户提供的信息。
- `computed`：实际 Keil 编译/链接、BIN 转换、MAP 对象/内存及 BIN 向量检查、15 项本机 C 故障模拟、原文件哈希复核。
- `md_extracted`：无；未使用 Markdown 抽取来判断公式、数值或硬件结果。
- `pdf_verified`：不适用；本任务没有 PDF 或其他需核对原始版式的源文档。
- 未解决风险：板型接线、ROM 启动/片内 RAM/时钟在实板上的行为、卡电源与信号完整性、真实 FatFs 分配、DMA/中断时序和性能均未验证。原 SDK 存在部分无墙钟总超时的硬件轮询；主机替身测试不覆盖这些路径。详见使用说明。

## 自审

确认五项命令存在；仅手动触发测试；唯一名称使用 CREATE_NEW；文件不删除；BASIC 同步关闭后重新打开；STRESS 16MiB 有界；统计与 CRC 在相同核心代码中运行；无下载/复位/串口通信动作。构建脚本已通过完整调用，验证脚本未把被链接器删除的 FatFs 函数误判为已链接功能。没有修改全局配置、技能或持久代理行为。

## 阶段结论

1. **是否具备烧录条件：** 构建产物、入口、Flash 基址及隔离证据齐备，可提交人工烧录审批；尚未获本阶段烧录授权，不能认定已通过板端启动或 SD 验收。
2. **是否通过独立构建：** 是，ARMCLANG 6.21，零错误零警告，日志/MAP/AXF/BIN 同次对应。
3. **下一阶段：** 人工批准烧录并核对固件/擦除范围后，另行授权复位及 COM14 通信；先核对 UART1 接线和 ready，再执行 STATUS、BASIC、RESULT，确认容量/文件/CRC；BASIC 成功后手动执行一次 STRESS、RESULT，保留完整串口日志与 SD 文件，断电后由人工独立核验文件内容。COM10 本固件不初始化，不能当成无线链路验收。
4. **原 C31 与未提交改动：** 已保护，基线复核零变化；不影响现有源码中的自启动实现，当前核心板镜像也没有被更改。
