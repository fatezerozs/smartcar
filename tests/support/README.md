> **维护口径（2026-11-09）：** `00bb7a4`、72定义、49 PASS/1 SKIPPED等数字属于2026-11-06被测版本，不能代表最新HEAD或未提交catalog。Phase2整合提交bd94f27另有41项Python与19350项模拟IO断言，本轮未核定该提交远程CI；它们不是统一Host/PreFlash套件计数，也不代替实车验收。 统一状态与历史适用范围见[文档维护总览](../../docs/DOCUMENTATION_STATUS.md)。

# Shared Host support

Fixtures under `fixtures/` own reusable clock, PWM, encoder, camera, UART and
route/switch models. A fixture contains no test entrypoint or scenario assertions
from another test. Each executable owns its own model state and explicit reset.

The existing platform stubs remain in `tests/vision/stubs/`: they are deliberately
not moved, preserving include ordering and compiler behavior. The historical
vision algorithm stays in `tests/vision/reference/`, linked as a separately renamed
object, never included in the target firmware.

Safety assertions in shared IO adapters remain active. Sharing a fixture never
means inheriting another executable's initialization or prior run.
