# SmartCar test report

Run: `20261111T154434-38617b63c9` · Profile: `Suite` · Status: **PASS** · Scenario acceptance: **NOT_EVALUATED** · Command passed: `True`

Git: `1cae353a1b1381912e821e9da5ca66b2c0a3cba5` · branch: `docs/main-documentation-20261107` · dirty: `True`

Start: 2026-11-11T15:44:34.464+00:00 · duration: 11.085 seconds

Software passed: `True` · release eligible within profile: `False`

Results apply only to the selected suites, macro variants, toolchain and recorded inputs. Quality/performance without a dataset is not measured.

| Suite | Layer / evidence | Required | Status | Seconds | Exit | Reason | Logs |
| --- | --- | --- | --- | --- | --- | --- | --- |
| environment | environment / host_simulated | True | PASS | 0.256 | 0 | Completed within declared software evidence scope | [stdout](suites/environment/stdout.log) / [stderr](suites/environment/stderr.log) |
| compiler-environment | environment / host_simulated | True | PASS | 0.197 | 0 | Completed within declared software evidence scope | [stdout](suites/compiler-environment/stdout.log) / [stderr](suites/compiler-environment/stderr.log) |
| minimal-track | integration / host_simulated | True | PASS | 10.034 | 0 | Completed within declared software evidence scope | [stdout](suites/minimal-track/stdout.log) / [stderr](suites/minimal-track/stderr.log) |

## Evidence and remaining scope

- Host simulation does not verify physical PWM/LDOK, MCU timing, DMA races or vehicle/track acceptance.
- Hardware acceptance and manual ISR/DMA/critical-section review remain pending.
- GCC was explicitly selected for supplemental Host validation; MSVC /W4 /WX and ASan results are separate evidence.
- This run does not claim the complete PreFlash target-build/static profile.
- Scenario continuous-metric-ring: NOT_EVALUATED — 本轮未执行该场景检查。

## Scenario acceptance

Only the scenario checks declared in this catalog; not physical vehicle, flashing or hardware authorization.

| Check | Suite | Status | Reason | Evidence |
| --- | --- | --- | --- | --- |
| continuous-metric-ring | vision-scene-integration | NOT_EVALUATED | 本轮未执行该场景检查。 | not available |

Tool versions and machine details: [environment.json](environment.json).
Macro combinations, source paths, process arguments and build variants: [run.json](run.json).
No history is counted as a result of this run. No firmware flashing or hardware operation was performed.
