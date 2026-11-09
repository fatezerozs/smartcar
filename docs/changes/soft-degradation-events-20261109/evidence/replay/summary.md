# SmartCar test report

Run: `20261109T100243-e1a183b3da` · Profile: `Suite` · Status: **PASS**

Git: `71338813afb555d1d2cdbe51ea4c491e4f14e68d` · branch: `docs/main-documentation-20261107` · dirty: `True`

Start: 2026-11-09T10:02:43.310+00:00 · duration: 8.092 seconds

Software passed: `True` · release eligible within profile: `False`

Results apply only to the selected suites, macro variants, toolchain and recorded inputs. Quality/performance without a dataset is not measured.

| Suite | Layer / evidence | Required | Status | Seconds | Exit | Reason | Logs |
| --- | --- | --- | --- | --- | --- | --- | --- |
| environment | environment / host_simulated | True | PASS | 0.228 | 0 | Completed within declared software evidence scope | [stdout](suites/environment/stdout.log) / [stderr](suites/environment/stderr.log) |
| compiler-environment | environment / host_simulated | True | PASS | 0.216 | 0 | Completed within declared software evidence scope | [stdout](suites/compiler-environment/stdout.log) / [stderr](suites/compiler-environment/stderr.log) |
| phase3-shadow-preparation | integration / host_simulated | True | PASS | 7.005 | 0 | Completed within declared software evidence scope | [stdout](suites/phase3-shadow-preparation/stdout.log) / [stderr](suites/phase3-shadow-preparation/stderr.log) |
| vision-image-regression | integration / host_simulated | True | PASS | 7.136 | 0 | Completed within declared software evidence scope | [stdout](suites/vision-image-regression/stdout.log) / [stderr](suites/vision-image-regression/stderr.log) |
| metric-odom-replay | integration / host_simulated | True | PASS | 0.985 | 0 | Completed within declared software evidence scope | [stdout](suites/metric-odom-replay/stdout.log) / [stderr](suites/metric-odom-replay/stderr.log) |
| planner-shadow-replay | integration / host_simulated | True | PASS | 6.545 | 0 | Completed within declared software evidence scope | [stdout](suites/planner-shadow-replay/stdout.log) / [stderr](suites/planner-shadow-replay/stderr.log) |

## Evidence and remaining scope

- Host simulation does not verify physical PWM/LDOK, MCU timing, DMA races or vehicle/track acceptance.
- Hardware acceptance and manual ISR/DMA/critical-section review remain pending.
- GCC was explicitly selected for supplemental Host validation; MSVC /W4 /WX and ASan results are separate evidence.
- This run does not claim the complete PreFlash target-build/static profile.

Tool versions and machine details: [environment.json](environment.json).
Macro combinations, source paths, process arguments and build variants: [run.json](run.json).
No history is counted as a result of this run. No firmware flashing or hardware operation was performed.
