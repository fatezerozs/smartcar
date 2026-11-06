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
