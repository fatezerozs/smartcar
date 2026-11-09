# SmartCar test report

Run: `20261109T010440-89dbb0ce8e` · Profile: `Suite` · Status: **PASS**

Git: `f7fb5b9b8cecda8c5d366b12dd9b0adb0599fd41` · branch: `docs/main-documentation-20261107` · dirty: `True`

Start: 2026-11-09T01:04:40.930+00:00 · duration: 2.245 seconds

Software passed: `True` · release eligible within profile: `False`

Results apply only to the selected suites, macro variants, toolchain and recorded inputs. Quality/performance without a dataset is not measured.

| Suite | Layer / evidence | Required | Status | Seconds | Exit | Reason | Logs |
| --- | --- | --- | --- | --- | --- | --- | --- |
| environment | environment / host_simulated | True | PASS | 0.295 | 0 | Completed within declared software evidence scope | [stdout](suites/environment/stdout.log) / [stderr](suites/environment/stderr.log) |
| static-safety | build / static | True | PASS | 1.146 | 0 | Completed within declared software evidence scope | [stdout](suites/static-safety/stdout.log) / [stderr](suites/static-safety/stderr.log) |
| project-structure | build / static | True | PASS | 1.250 | 0 | Completed within declared software evidence scope | [stdout](suites/project-structure/stdout.log) / [stderr](suites/project-structure/stderr.log) |

## Evidence and remaining scope

- Host simulation does not verify physical PWM/LDOK, MCU timing, DMA races or vehicle/track acceptance.
- Hardware acceptance and manual ISR/DMA/critical-section review remain pending.
- GCC was explicitly selected for supplemental Host validation; MSVC /W4 /WX and ASan results are separate evidence.
- This run does not claim the complete PreFlash target-build/static profile.

Tool versions and machine details: [environment.json](environment.json).
Macro combinations, source paths, process arguments and build variants: [run.json](run.json).
No history is counted as a result of this run. No firmware flashing or hardware operation was performed.
