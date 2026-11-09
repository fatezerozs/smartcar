> **维护口径（2026-11-09）：** `00bb7a4`、72定义、49 PASS/1 SKIPPED等数字属于2026-11-06被测版本，不能代表最新HEAD或未提交catalog。Phase2整合提交bd94f27另有41项Python与19350项模拟IO断言，本轮未核定该提交远程CI；它们不是统一Host/PreFlash套件计数，也不代替实车验收。 统一状态与历史适用范围见[文档维护总览](../../docs/DOCUMENTATION_STATUS.md)。

# Suite catalog

`catalog.json` is the single source of suite IDs, ownership, variants, source lists,
macros, profiles, prerequisites, timeouts, resource locks and evidence classes.
It is validated against `../schemas/suite.schema.json` with PowerShell 7 `Test-Json`.

- L0: `environment`, `compiler-environment`, `target-environment`, `arm-environment`.
- L1: Vision, Bayes, Control, Drive, Speed, Link, board adapters and offline tools.
- L2: Debug/runtime, route/switch/turn, midterm, auto-track and motion chains.
- L2: `speed-planning-host` checks the isolated synthetic geometry/planning/jerk,
  near/far supervisor and offline sidecar/budget contracts. It has no production
  defaults or MCU integration. The historical ID `c26-baseline-characterization`
  now checks C26 admission, physical OFF acknowledgement, severe-fault retention
  and near-invalid motor inhibition after the first Phase 3 changes; it is not
  acceptance of the calibrated planner. See
  [the stage report](../../docs/changes/c31-speed-planning-20261105/README.md).
- L2: `c26-snapshot` exercises real output/PI modules with simulated interrupt
  interleavings, request generations, sample/lease invalidation, immutable
  geometry preparation and IRQ-mask restoration. It does not measure WCET,
  electrical PWM timing or calibrated planning/physical stopping performance.
- L2: `planner-interface` exercises the shared metric planner through the MCU
  shadow API, including data/identity/age checks, stop semantics and explicit
  counts/s to counts/10ms quantization. Its target code is compiled in the main
  project; the primary defaults disable its optional runtime.
  `invalid-configuration` checks the active planner guard; no production
  parameters or physical actuator authorization are added by these tests.
- L2: `planner-shadow` exercises the serialized supervisor/trajectory composition
  and capture-aligned horizon advancement. `planner-shadow-replay` verifies 750
  native visual records and replays 5 scenarios with explicitly synthetic motion
  and metric geometry; it provides no PI/PWM, live scheduling or hardware result.
- L2: `metric-control` connects native cumulative snapshots, bounded odometry
  history, the existing paired-row vision adapter and shared shadow planner to
  caller-owned diagnostic instances of the unchanged wheel PI. It includes
  synthetic closed-loop plants. `metric-odom-replay` checks 1516 frozen count
  records against an independent conversion/interpolation oracle; wheel scales
  are explicit candidates and capture queries are synthetic midpoint times.
- L2: `planner-runtime` runs both control and shadow macro variants through
  actual C26 scheduling, the original live PI instances and the sole writer.
  Synthetic geometry/scales exercise direct counts, independent PWM cap,
  near/far transitions, held-ON latches, IRQ interleavings and commit deadlines.
  The test-only review macro is not physical calibration or board timing evidence.
- L2: `phase3-shadow-preparation` pins measured Phase2 inputs and labelled
  reference values, checks preview/raw pixel and mm/metre transforms, builds
  an isolated candidate, compares legacy PWM sequences, exercises stop-only
  retained rows, and replays all 750 real images to expose geometry coverage.
- L2: `vision-image-regression` verifies all 750 original full-resolution image
  CRC/SHA records, native C vision/metric coverage and 28 locally reviewed
  paired boundary bands. These bands were selected after algorithm experiments;
  they are not a complete accuracy benchmark or an independent holdout.
- L2: `recorder` adds 17 capture/storage/SCRAW synthetic integration cases, using
  the selected Host compiler/mode; `recorder-tools` checks incremental generation
  without starting a firmware build. Both are required in Host and PreFlash.
- L3-A: Vision preprocessing/oracle, field protocol, transport, interface/data,
  valid macro matrix and exact-diagnostic invalid macro rejection.
- L3-B: `performance-host`; pending inputs are an optional, explicit `SKIPPED`.
- L4: Project/source/static checks, ARM source compilation and nine isolated MDK builds.
- L5: `hardware-acceptance`; manual evidence only, excluded from every automatic profile.

Aggregate IDs (`core`, `link-group`, `speed-group`, `midterm`, `route`, `switch`,
`equivalence`) preserve legacy script groupings. They have no implicit initialization
or shared executable state. Their children can all run independently.

The historical vision reference is only an oracle for the explicitly documented
preprocessing contract. Tracking and overlays are outside its equality promise.
