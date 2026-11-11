# MPC_R13_MINIMAL_20261111_1：GitHub 下载与复现

这是 R13 最小赛道软件候选，保留后续 MPC 接入的职责边界；当前不运行 MPC 求解器、米制或影子链。C26 启停，保留 STOP、故障、新鲜度保护与原最终输出。未烧录、未发车，两处环形各一圈和返回 A 的连续全程尚未实车验收。

- [候选 Release 和下载](https://github.com/fatezerozs/smartcar/releases/tag/MPC_R13_MINIMAL_20261111_1)：源码 ZIP、AXF、BIN、MAP、哈希和中文记录。
- [版本化应用与再生说明](../../../demo/candidates/minimal-track/README.md)
- [已封存验证报告](../../../releases/MPC_R13_MINIMAL_20261111_1/README.md)
- [上传前独立副本回归](validation-upload/user-report.md)：统一入口 3 PASS，内部 6 项通过；场景验收未执行，未操作硬件。
- [最终专项报告](../../../releases/MPC_R13_MINIMAL_20261111_1/validation-final/user-report.md)
- [赛道目标](../planner-authority-design-20261111/MINIMAL_TRACK_PROPOSAL.md)

## 在新目录生成工程

Windows 下安装 GCC 与 Keil 后，在本分支仓库根目录运行：

```powershell
python tools/prepare_minimal_track.py --output .build/mpc-r13-minimal-local --build
python tools/testing/run_tests.py --profile Host --suite minimal-track --compiler gcc --jobs 1
```

Target：`nor_sdram_zf_dtcm`。生成的工程在 `.build/mpc-r13-minimal-local/workspace/demo/mdk/competition/`：普通构建入口 `MPC_R13_Minimal.uvprojx`、下载入口 `MPC_R13_Minimal_Download.uvprojx`。仅生成和编译不会烧录或启动车辆。

也可下载 Release 的 `source-code.zip` 解包，再用同一生成命令构建。ZIP 自带冻结依赖和专项测试，可直接运行 `python tests/vision/test_minimal_track.py`；ZIP 不包含整个仓库的统一测试框架。

历史验证报告中的 `C:\...`、`.build` 和 `.test-results` 指当时本机证据位置，不是 GitHub 上的现成工程入口。请按上述命令再生工程；完整工作区 Host 的历史报告未随该独立版本上传，其 70 PASS / 1 SKIPPED 仅作为历史背景。最终候选的专项输出、源码包复现及固件身份已封存。

本次上传保持原候选源码、固件及封存记录的哈希。为使 Git 克隆后严格的依赖哈希仍能通过，仅对四个已锁定依赖增加原始字节保留规则：基础 uvoptx、RT106X_config.h 和两份 FreeRTOS 许可/来源文档；配置头和文档内容没有算法变化。上传不包含原工作区其他未提交实现。
