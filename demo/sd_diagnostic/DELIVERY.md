> **维护口径（2026-11-09）：** 本文独立SD工程的已验收事实与历史输入保持不变；Phase2最新诊断没有SD运动联合验收，也未更新本工程发布镜像。跨工程的当前进度与入口由统一维护总览区分。 统一状态与历史适用范围见[文档维护总览](../../docs/DOCUMENTATION_STATUS.md)。

# 交付索引

- 独立 Keil 工程：[mdk/sd_diagnostic.uvprojx](mdk/sd_diagnostic.uvprojx)
- 使用说明：[README.md](README.md)
- 完整验收报告：[ACCEPTANCE.md](ACCEPTANCE.md)
- 源码审计：[evidence/source-audit.md](evidence/source-audit.md)
- 主机模拟报告：[tests/README.md](tests/README.md)
- 主机模拟日志：[evidence/host-tests-final.log](evidence/host-tests-final.log)
- COM 枚举和用户确认映射：[evidence/com-registry.txt](evidence/com-registry.txt)
- 原改动保护：[evidence/preservation-final.json](evidence/preservation-final.json)
- 全部新增文件清单与 SHA256：`MANIFEST.json`（清单自身除外）

最终构建目录：`artifacts/run-20261106T001438910-29796/`。

- [AXF](artifacts/run-20261106T001438910-29796/Objects/sd_diagnostic.axf)
- [BIN](artifacts/run-20261106T001438910-29796/Objects/sd_diagnostic.bin)
- [MAP](artifacts/run-20261106T001438910-29796/Listings/sd_diagnostic.map)
- [Keil 构建日志](artifacts/run-20261106T001438910-29796/keil-build.log)
- [构建、隔离、内存与输入哈希证据](artifacts/run-20261106T001438910-29796/verification.json)

源码：`diag_core.c` 为协议/测试/CRC/统计，`board.c` 为裸机 UART/SD 板级和 FatFs diskio，`diag_system.c` 为片内启动，`osa.c` 为单线程 SD OSA，`sd_policy.c`/`host_policy.c` 为原 SDK 编译策略，`include/diag.h`/`ffconf.h` 为诊断接口和私有 FatFs 配置，`mdk/sd_diagnostic.scf` 为独立布局。
构建与检查：`build.ps1`、`configure.py`、`verify.py`。主机测试：`tests/test_core.c`。
早期失败尝试和中间成功构建保留，只有上述最终目录作为本次验收交付固件。
