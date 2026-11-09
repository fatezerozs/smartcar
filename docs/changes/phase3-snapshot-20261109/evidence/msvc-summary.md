# SmartCar test report

Run: `20261108T210953-c9a1460a8d` · Profile: `Suite` · Status: **BLOCKED**

Git: `f7fb5b9b8cecda8c5d366b12dd9b0adb0599fd41` · branch: `docs/main-documentation-20261107` · dirty: `True`

Start: 2026-11-08T21:09:53.842+00:00 · duration: 0.886 seconds

Software passed: `False` · release eligible within profile: `False`

Results apply only to the selected suites, macro variants, toolchain and recorded inputs. Quality/performance without a dataset is not measured.

| Suite | Layer / evidence | Required | Status | Seconds | Exit | Reason | Logs |
| --- | --- | --- | --- | --- | --- | --- | --- |
| environment | environment / host_simulated | True | PASS | 0.311 | 0 | Completed within declared software evidence scope | [stdout](suites/environment/stdout.log) / [stderr](suites/environment/stderr.log) |
| compiler-environment | environment / host_simulated | True | BLOCKED | 0.013 | 3 | MSVC unavailable; install C++ Build Tools or supply -VcVars / SMARTCAR_VCVARS | [stdout](suites/compiler-environment/stdout.log) / [stderr](suites/compiler-environment/stderr.log) |
| c26-snapshot | integration / host_simulated | True | BLOCKED | 0.001 | 3 | Unmet dependencies: compiler-environment:BLOCKED | [stdout](suites/c26-snapshot/stdout.log) / [stderr](suites/c26-snapshot/stderr.log) |
| c26-baseline-characterization | integration / host_simulated | True | BLOCKED | 0.002 | 3 | Unmet dependencies: compiler-environment:BLOCKED | [stdout](suites/c26-baseline-characterization/stdout.log) / [stderr](suites/c26-baseline-characterization/stderr.log) |
| vision | unit / host_simulated | True | BLOCKED | 0.001 | 3 | Unmet dependencies: compiler-environment:BLOCKED | [stdout](suites/vision/stdout.log) / [stderr](suites/vision/stderr.log) |
| control | unit / host_simulated | True | BLOCKED | 0.000 | 3 | Unmet dependencies: compiler-environment:BLOCKED | [stdout](suites/control/stdout.log) / [stderr](suites/control/stderr.log) |
| speed | unit / host_simulated | True | BLOCKED | 0.000 | 3 | Unmet dependencies: compiler-environment:BLOCKED | [stdout](suites/speed/stdout.log) / [stderr](suites/speed/stderr.log) |
| auto-track | integration / host_simulated | True | BLOCKED | 0.000 | 3 | Unmet dependencies: compiler-environment:BLOCKED, vision:BLOCKED, control:BLOCKED, speed:BLOCKED | [stdout](suites/auto-track/stdout.log) / [stderr](suites/auto-track/stderr.log) |

## Evidence and remaining scope

- Host simulation does not verify physical PWM/LDOK, MCU timing, DMA races or vehicle/track acceptance.
- Hardware acceptance and manual ISR/DMA/critical-section review remain pending.
- This run does not claim the complete PreFlash target-build/static profile.
- compiler-environment: BLOCKED — MSVC unavailable; install C++ Build Tools or supply -VcVars / SMARTCAR_VCVARS
- c26-snapshot: BLOCKED — Unmet dependencies: compiler-environment:BLOCKED
- c26-baseline-characterization: BLOCKED — Unmet dependencies: compiler-environment:BLOCKED
- vision: BLOCKED — Unmet dependencies: compiler-environment:BLOCKED
- control: BLOCKED — Unmet dependencies: compiler-environment:BLOCKED
- speed: BLOCKED — Unmet dependencies: compiler-environment:BLOCKED
- auto-track: BLOCKED — Unmet dependencies: compiler-environment:BLOCKED, vision:BLOCKED, control:BLOCKED, speed:BLOCKED

Tool versions and machine details: [environment.json](environment.json).
Macro combinations, source paths, process arguments and build variants: [run.json](run.json).
No history is counted as a result of this run. No firmware flashing or hardware operation was performed.
