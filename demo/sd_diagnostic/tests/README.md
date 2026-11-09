> **维护口径（2026-11-09）：** 本文独立SD工程的已验收事实与历史输入保持不变；Phase2最新诊断没有SD运动联合验收，也未更新本工程发布镜像。跨工程的当前进度与入口由统一维护总览区分。 统一状态与历史适用范围见[文档维护总览](../../../docs/DOCUMENTATION_STATUS.md)。

# 主机模拟测试

编译并执行的是同一份 `diag_core.c`；FatFs API 使用内存故障注入替身。没有设备/COM/磁盘访问。

```powershell
gcc -std=c99 -Wall -Wextra -Werror -IC:/shengchanli/smartCar/smartcar/demo/sd_diagnostic/include -IC:/shengchanli/smartCar/smartcar/libraries/components/fatfs/source C:/shengchanli/smartCar/smartcar/demo/sd_diagnostic/tests/test_core.c -o C:/shengchanli/smartCar/smartcar/demo/sd_diagnostic/tests/test_core-retest.exe
C:/shengchanli/smartCar/smartcar/demo/sd_diagnostic/tests/test_core-retest.exe
```

最终日志：`../evidence/host-tests-final.log`。GCC 的 `-Wall -Wextra -Werror` 编译通过，15 项检查全部通过：

1. CRC32 标准向量 `123456789` = `0xCBF43926`。
2. 无 BASIC 成功记录时拒绝 STRESS，写入次数为零。
3. 挂载失败。
4. 短写。
5. 回读数据损坏触发 CRC 失败。
6. 同步失败。
7. 关闭失败。
8. 回读重新打开失败。
9. 短读。
10. 剩余空间不足。
11. 已有三个同名文件时通过 CREATE_NEW 递增到新名称。
12. 写入 API 返回错误。
13. BASIC 64KiB 成功，检查写/读次数以及 sync/close/reopen 次序路径。
14. STRESS 16MiB 成功，检查有界块数、百分位值及重新门控。
15. HELP/STATUS/RESULT/未知命令路由；这些命令不写测试文件。

结果可信层为 computed（本机编译运行）。这些检查不执行真实 FatFs FAT 分配、不执行 SD Host/USDHC 寄存器代码、不模拟 DMA/中断时序，也不证明 SD 识别、初始化、写入、回读 CRC 或性能成功。实际 SD 验收全部待板端测试。
