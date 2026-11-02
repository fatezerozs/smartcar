# Suite catalog

`catalog.json` is the single source of suite IDs, ownership, variants, source lists,
macros, profiles, prerequisites, timeouts, resource locks and evidence classes.
It is validated against `../schemas/suite.schema.json` with PowerShell 7 `Test-Json`.

- L0: `environment`, `compiler-environment`, `target-environment`, `arm-environment`.
- L1: Vision, Bayes, Control, Drive, Speed, Link, board adapters and offline tools.
- L2: Debug/runtime, route/switch/turn, midterm, auto-track and motion chains.
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
