> **维护口径（2026-11-09）：** `00bb7a4`、72定义、49 PASS/1 SKIPPED等数字属于2026-11-06被测版本，不能代表最新HEAD或未提交catalog。Phase2整合提交bd94f27另有41项Python与19350项模拟IO断言，本轮未核定该提交远程CI；它们不是统一Host/PreFlash套件计数，也不代替实车验收。 统一状态与历史适用范围见[文档维护总览](../../docs/DOCUMENTATION_STATUS.md)。

# Standalone offline data-pipeline tests

Run from the repository root:

```powershell
python -m unittest discover -s tests/vision_data -p test_pipeline.py -v
```

Uses Python standard-library temporary directories, synthetic pixels and injected
MockTransport. The production-vision consumer test needs GCC C11. No hardware or
existing Label Studio instance is contacted. One HTTP test creates a temporary
loopback-only read-only image server and tears it down; it tests only synthetic
selected assets, traversal rejection, unselected images and unsupported write verbs.

Synthetic review actors exercise transitions; these are not actual human reviews.
The tests do not replace C32's original Equivalence/Host/PreFlash suites. The existing
catalog, Runner and CI are unchanged; this is an independently invoked consumer suite.
