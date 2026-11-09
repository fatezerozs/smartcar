# SmartCar test report

Run: `20261109T035321-ea96a23b4d` · Profile: `Suite` · Status: **PASS**

Git: `71338813afb555d1d2cdbe51ea4c491e4f14e68d` · branch: `docs/main-documentation-20261107` · dirty: `True`

Start: 2026-11-09T03:53:21.511+00:00 · duration: 1.495 seconds

Software passed: `True` · release eligible within profile: `False`

Results apply only to the selected suites, macro variants, toolchain and recorded inputs. Quality/performance without a dataset is not measured.

| Suite | Layer / evidence | Required | Status | Seconds | Exit | Reason | Logs |
| --- | --- | --- | --- | --- | --- | --- | --- |
| environment | environment / host_simulated | True | PASS | 0.239 | 0 | Completed within declared software evidence scope | [stdout](suites/environment/stdout.log) / [stderr](suites/environment/stderr.log) |
| static-safety | build / static | True | PASS | 0.659 | 0 | Completed within declared software evidence scope | [stdout](suites/static-safety/stdout.log) / [stderr](suites/static-safety/stderr.log) |
| project-structure | build / static | True | PASS | 0.727 | 0 | Completed within declared software evidence scope | [stdout](suites/project-structure/stdout.log) / [stderr](suites/project-structure/stderr.log) |

## Evidence and remaining scope

- Host simulation does not verify physical PWM/LDOK, MCU timing, DMA races or vehicle/track acceptance.
- Hardware acceptance and manual ISR/DMA/critical-section review remain pending.
- GCC was explicitly selected for supplemental Host validation; MSVC /W4 /WX and ASan results are separate evidence.
- This run does not claim the complete PreFlash target-build/static profile.

Tool versions and machine details: [environment.json](environment.json).
Macro combinations, source paths, process arguments and build variants: [run.json](run.json).
No history is counted as a result of this run. No firmware flashing or hardware operation was performed.
