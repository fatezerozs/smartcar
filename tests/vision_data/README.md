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
