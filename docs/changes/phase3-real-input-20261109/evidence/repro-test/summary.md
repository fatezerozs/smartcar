# SmartCar test report

Run: `20261109T031557-2f6a3b1378` · Profile: `Suite` · Status: **FAIL**

Git: `71338813afb555d1d2cdbe51ea4c491e4f14e68d` · branch: `docs/main-documentation-20261107` · dirty: `True`

Start: 2026-11-09T03:15:57.028+00:00 · duration: 2.048 seconds

Software passed: `False` · release eligible within profile: `False`

Results apply only to the selected suites, macro variants, toolchain and recorded inputs. Quality/performance without a dataset is not measured.

| Suite | Layer / evidence | Required | Status | Seconds | Exit | Reason | Logs |
| --- | --- | --- | --- | --- | --- | --- | --- |
| environment | environment / host_simulated | True | PASS | 0.322 | 0 | Completed within declared software evidence scope | [stdout](suites/environment/stdout.log) / [stderr](suites/environment/stderr.log) |
| compiler-environment | environment / host_simulated | True | PASS | 0.253 | 0 | Completed within declared software evidence scope | [stdout](suites/compiler-environment/stdout.log) / [stderr](suites/compiler-environment/stderr.log) |
| planner-shadow | integration / host_simulated | True | FAIL | 0.855 | 1 | test exited with code 1 | [stdout](suites/planner-shadow/stdout.log) / [stderr](suites/planner-shadow/stderr.log) |

## Evidence and remaining scope

- Host simulation does not verify physical PWM/LDOK, MCU timing, DMA races or vehicle/track acceptance.
- Hardware acceptance and manual ISR/DMA/critical-section review remain pending.
- GCC was explicitly selected for supplemental Host validation; MSVC /W4 /WX and ASan results are separate evidence.
- This run does not claim the complete PreFlash target-build/static profile.
- planner-shadow: FAIL — test exited with code 1

Tool versions and machine details: [environment.json](environment.json).
Macro combinations, source paths, process arguments and build variants: [run.json](run.json).
No history is counted as a result of this run. No firmware flashing or hardware operation was performed.
