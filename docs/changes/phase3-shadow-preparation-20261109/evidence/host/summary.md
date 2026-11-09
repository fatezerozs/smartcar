# SmartCar test report

Run: `20261109T020349-ab0a2b1ce4` · Profile: `Host` · Status: **PASS**

Git: `bd94f271e47bbdd0c80bce6e409e7acbad232a81` · branch: `docs/main-documentation-20261107` · dirty: `True`

Start: 2026-11-09T02:03:49.116+00:00 · duration: 50.057 seconds

Software passed: `True` · release eligible within profile: `False`

Results apply only to the selected suites, macro variants, toolchain and recorded inputs. Quality/performance without a dataset is not measured.

| Suite | Layer / evidence | Required | Status | Seconds | Exit | Reason | Logs |
| --- | --- | --- | --- | --- | --- | --- | --- |
| environment | environment / host_simulated | True | PASS | 0.281 | 0 | Completed within declared software evidence scope | [stdout](suites/environment/stdout.log) / [stderr](suites/environment/stderr.log) |
| compiler-environment | environment / host_simulated | True | PASS | 0.230 | 0 | Completed within declared software evidence scope | [stdout](suites/compiler-environment/stdout.log) / [stderr](suites/compiler-environment/stderr.log) |
| vision | unit / host_simulated | True | PASS | 1.298 | 0 | Completed within declared software evidence scope | [stdout](suites/vision/stdout.log) / [stderr](suites/vision/stderr.log) |
| control | unit / host_simulated | True | PASS | 0.637 | 0 | Completed within declared software evidence scope | [stdout](suites/control/stdout.log) / [stderr](suites/control/stderr.log) |
| bayes | unit / host_simulated | True | PASS | 0.833 | 0 | Completed within declared software evidence scope | [stdout](suites/bayes/stdout.log) / [stderr](suites/bayes/stderr.log) |
| field | contract / host_simulated | True | PASS | 1.591 | 0 | Completed within declared software evidence scope | [stdout](suites/field/stdout.log) / [stderr](suites/field/stderr.log) |
| servo-probe | unit / host_simulated | True | PASS | 1.394 | 0 | Completed within declared software evidence scope | [stdout](suites/servo-probe/stdout.log) / [stderr](suites/servo-probe/stderr.log) |
| motor-probe | unit / host_simulated | True | PASS | 1.348 | 0 | Completed within declared software evidence scope | [stdout](suites/motor-probe/stdout.log) / [stderr](suites/motor-probe/stderr.log) |
| startup | integration / host_simulated | True | PASS | 1.449 | 0 | Completed within declared software evidence scope | [stdout](suites/startup/stdout.log) / [stderr](suites/startup/stderr.log) |
| board-mode | unit / host_simulated | True | PASS | 1.310 | 0 | Completed within declared software evidence scope | [stdout](suites/board-mode/stdout.log) / [stderr](suites/board-mode/stderr.log) |
| drive | unit / host_simulated | True | PASS | 3.732 | 0 | Completed within declared software evidence scope | [stdout](suites/drive/stdout.log) / [stderr](suites/drive/stderr.log) |
| debug | integration / host_simulated | True | PASS | 6.981 | 0 | Completed within declared software evidence scope | [stdout](suites/debug/stdout.log) / [stderr](suites/debug/stderr.log) |
| link | unit / host_simulated | True | PASS | 0.310 | 0 | Completed within declared software evidence scope | [stdout](suites/link/stdout.log) / [stderr](suites/link/stderr.log) |
| link-board | unit / host_simulated | True | PASS | 0.369 | 0 | Completed within declared software evidence scope | [stdout](suites/link-board/stdout.log) / [stderr](suites/link-board/stderr.log) |
| transport | contract / host_simulated | True | PASS | 0.427 | 0 | Completed within declared software evidence scope | [stdout](suites/transport/stdout.log) / [stderr](suites/transport/stderr.log) |
| dual-runtime | integration / host_simulated | True | PASS | 2.421 | 0 | Completed within declared software evidence scope | [stdout](suites/dual-runtime/stdout.log) / [stderr](suites/dual-runtime/stderr.log) |
| speed | unit / host_simulated | True | PASS | 1.819 | 0 | Completed within declared software evidence scope | [stdout](suites/speed/stdout.log) / [stderr](suites/speed/stderr.log) |
| encoder-board | unit / host_simulated | True | PASS | 0.276 | 0 | Completed within declared software evidence scope | [stdout](suites/encoder-board/stdout.log) / [stderr](suites/encoder-board/stderr.log) |
| route-trial | integration / host_simulated | True | PASS | 1.831 | 0 | Completed within declared software evidence scope | [stdout](suites/route-trial/stdout.log) / [stderr](suites/route-trial/stderr.log) |
| route-switch | integration / host_simulated | True | PASS | 2.606 | 0 | Completed within declared software evidence scope | [stdout](suites/route-switch/stdout.log) / [stderr](suites/route-switch/stderr.log) |
| switch-runtime | integration / host_simulated | True | PASS | 4.234 | 0 | Completed within declared software evidence scope | [stdout](suites/switch-runtime/stdout.log) / [stderr](suites/switch-runtime/stderr.log) |
| turn-route | integration / host_simulated | True | PASS | 9.564 | 0 | Completed within declared software evidence scope | [stdout](suites/turn-route/stdout.log) / [stderr](suites/turn-route/stderr.log) |
| roundabout | unit / host_simulated | True | PASS | 1.410 | 0 | Completed within declared software evidence scope | [stdout](suites/roundabout/stdout.log) / [stderr](suites/roundabout/stderr.log) |
| roundabout-projection | unit / host_simulated | True | PASS | 6.612 | 0 | Completed within declared software evidence scope | [stdout](suites/roundabout-projection/stdout.log) / [stderr](suites/roundabout-projection/stderr.log) |
| tangent-preview | contract / host_simulated | True | PASS | 1.657 | 0 | Completed within declared software evidence scope | [stdout](suites/tangent-preview/stdout.log) / [stderr](suites/tangent-preview/stderr.log) |
| midterm-boot | integration / host_simulated | True | PASS | 3.025 | 0 | Completed within declared software evidence scope | [stdout](suites/midterm-boot/stdout.log) / [stderr](suites/midterm-boot/stderr.log) |
| midterm-runtime | integration / host_simulated | True | PASS | 2.614 | 0 | Completed within declared software evidence scope | [stdout](suites/midterm-runtime/stdout.log) / [stderr](suites/midterm-runtime/stderr.log) |
| auto-track | integration / host_simulated | True | PASS | 7.386 | 0 | Completed within declared software evidence scope | [stdout](suites/auto-track/stdout.log) / [stderr](suites/auto-track/stderr.log) |
| motion | integration / host_simulated | True | PASS | 2.749 | 0 | Completed within declared software evidence scope | [stdout](suites/motion/stdout.log) / [stderr](suites/motion/stderr.log) |
| motor-threshold | integration / host_simulated | True | PASS | 2.617 | 0 | Completed within declared software evidence scope | [stdout](suites/motor-threshold/stdout.log) / [stderr](suites/motor-threshold/stderr.log) |
| vision-contract | contract / host_simulated | True | PASS | 11.059 | 0 | Completed within declared software evidence scope | [stdout](suites/vision-contract/stdout.log) / [stderr](suites/vision-contract/stderr.log) |
| capture-vision | unit / host_simulated | True | PASS | 6.042 | 0 | Completed within declared software evidence scope | [stdout](suites/capture-vision/stdout.log) / [stderr](suites/capture-vision/stderr.log) |
| fit-model | unit / host_simulated | True | PASS | 0.165 | 0 | Completed within declared software evidence scope | [stdout](suites/fit-model/stdout.log) / [stderr](suites/fit-model/stderr.log) |
| restore-training-parameters | unit / host_simulated | True | PASS | 0.331 | 0 | Completed within declared software evidence scope | [stdout](suites/restore-training-parameters/stdout.log) / [stderr](suites/restore-training-parameters/stderr.log) |
| run-analysis | unit / host_simulated | True | PASS | 0.153 | 0 | Completed within declared software evidence scope | [stdout](suites/run-analysis/stdout.log) / [stderr](suites/run-analysis/stderr.log) |
| seekfree-import | unit / host_simulated | True | PASS | 0.264 | 0 | Completed within declared software evidence scope | [stdout](suites/seekfree-import/stdout.log) / [stderr](suites/seekfree-import/stderr.log) |
| track-trials | unit / host_simulated | True | PASS | 0.736 | 0 | Completed within declared software evidence scope | [stdout](suites/track-trials/stdout.log) / [stderr](suites/track-trials/stderr.log) |
| training-analysis | unit / host_simulated | True | PASS | 0.143 | 0 | Completed within declared software evidence scope | [stdout](suites/training-analysis/stdout.log) / [stderr](suites/training-analysis/stderr.log) |
| field-protocol | contract / host_simulated | True | PASS | 0.449 | 0 | Completed within declared software evidence scope | [stdout](suites/field-protocol/stdout.log) / [stderr](suites/field-protocol/stderr.log) |
| configuration-contract | contract / host_simulated | True | PASS | 1.155 | 0 | Completed within declared software evidence scope | [stdout](suites/configuration-contract/stdout.log) / [stderr](suites/configuration-contract/stderr.log) |
| invalid-configuration | contract / host_simulated | True | PASS | 0.429 | 0 | Completed within declared software evidence scope | [stdout](suites/invalid-configuration/stdout.log) / [stderr](suites/invalid-configuration/stderr.log) |
| interface-contract | contract / host_simulated | True | PASS | 0.182 | 0 | Completed within declared software evidence scope | [stdout](suites/interface-contract/stdout.log) / [stderr](suites/interface-contract/stderr.log) |
| performance-host | performance / pq_host | False | SKIPPED | 0.486 | 0 | Dataset, versioned ground truth, measurement adapter and approved baseline await later integration; other suites remain independent. | [stdout](suites/performance-host/stdout.log) / [stderr](suites/performance-host/stderr.log) |
| recorder-tools | contract / host | True | PASS | 0.940 | 0 | Completed within declared software evidence scope | [stdout](suites/recorder-tools/stdout.log) / [stderr](suites/recorder-tools/stderr.log) |
| recorder | integration / host_simulated | True | PASS | 2.675 | 0 | Completed within declared software evidence scope | [stdout](suites/recorder/stdout.log) / [stderr](suites/recorder/stderr.log) |
| framework | unit / host | True | PASS | 11.882 | 0 | Completed within declared software evidence scope | [stdout](suites/framework/stdout.log) / [stderr](suites/framework/stderr.log) |
| recorder-startup | integration / host_simulated | True | PASS | 6.888 | 0 | Completed within declared software evidence scope | [stdout](suites/recorder-startup/stdout.log) / [stderr](suites/recorder-startup/stderr.log) |
| recorder-sd-pins | unit / host_simulated | True | PASS | 0.307 | 0 | Completed within declared software evidence scope | [stdout](suites/recorder-sd-pins/stdout.log) / [stderr](suites/recorder-sd-pins/stderr.log) |
| speed-planning-host | integration / host_simulated | True | PASS | 3.034 | 0 | Completed within declared software evidence scope | [stdout](suites/speed-planning-host/stdout.log) / [stderr](suites/speed-planning-host/stderr.log) |
| speed-planning-visual-trace | integration / host_simulated | True | PASS | 0.970 | 0 | Completed within declared software evidence scope | [stdout](suites/speed-planning-visual-trace/stdout.log) / [stderr](suites/speed-planning-visual-trace/stderr.log) |
| c26-baseline-characterization | integration / host_simulated | True | PASS | 2.092 | 0 | Completed within declared software evidence scope | [stdout](suites/c26-baseline-characterization/stdout.log) / [stderr](suites/c26-baseline-characterization/stderr.log) |
| c26-snapshot | integration / host_simulated | True | PASS | 1.995 | 0 | Completed within declared software evidence scope | [stdout](suites/c26-snapshot/stdout.log) / [stderr](suites/c26-snapshot/stderr.log) |
| planner-interface | integration / host_simulated | True | PASS | 0.644 | 0 | Completed within declared software evidence scope | [stdout](suites/planner-interface/stdout.log) / [stderr](suites/planner-interface/stderr.log) |
| planner-shadow | integration / host_simulated | True | PASS | 0.843 | 0 | Completed within declared software evidence scope | [stdout](suites/planner-shadow/stdout.log) / [stderr](suites/planner-shadow/stderr.log) |
| planner-shadow-replay | integration / host_simulated | True | PASS | 2.085 | 0 | Completed within declared software evidence scope | [stdout](suites/planner-shadow-replay/stdout.log) / [stderr](suites/planner-shadow-replay/stderr.log) |
| metric-control | integration / host_simulated | True | PASS | 1.191 | 0 | Completed within declared software evidence scope | [stdout](suites/metric-control/stdout.log) / [stderr](suites/metric-control/stderr.log) |
| metric-odom-replay | integration / host_simulated | True | PASS | 0.893 | 0 | Completed within declared software evidence scope | [stdout](suites/metric-odom-replay/stdout.log) / [stderr](suites/metric-odom-replay/stderr.log) |
| planner-runtime | integration / host_simulated | True | PASS | 5.240 | 0 | Completed within declared software evidence scope | [stdout](suites/planner-runtime/stdout.log) / [stderr](suites/planner-runtime/stderr.log) |
| phase3-shadow-preparation | integration / host_simulated | True | PASS | 5.729 | 0 | Completed within declared software evidence scope | [stdout](suites/phase3-shadow-preparation/stdout.log) / [stderr](suites/phase3-shadow-preparation/stderr.log) |

## Evidence and remaining scope

- Host simulation does not verify physical PWM/LDOK, MCU timing, DMA races or vehicle/track acceptance.
- Hardware acceptance and manual ISR/DMA/critical-section review remain pending.
- GCC was explicitly selected for supplemental Host validation; MSVC /W4 /WX and ASan results are separate evidence.
- This run does not claim the complete PreFlash target-build/static profile.
- performance-host: SKIPPED — Dataset, versioned ground truth, measurement adapter and approved baseline await later integration; other suites remain independent.

Tool versions and machine details: [environment.json](environment.json).
Macro combinations, source paths, process arguments and build variants: [run.json](run.json).
No history is counted as a result of this run. No firmware flashing or hardware operation was performed.
