# B2 + B3 stationary joint candidate: source audit in progress

Status: NOT BUILT / NOT FLASHED / NOT HARDWARE ACCEPTED. Stage 1 Encoder measurement has priority.

Evidence layers: native C/Python/XML sources read directly (`structured_parsed`); hardware binary CRC and stable counter comparison are `computed`. No PDF or Markdown extraction used as numerical evidence. Operator complete turns remain human observations.

- Primary C26 owner is `demo/mdk/competition/midterm_switch.uvprojx`: AUTO=0, ROUTE_SWITCH=1, ROUTE_TRIAL=1, ENC=1. Preserve original switch/fault acknowledgement and PI implementations through private instrumentation adapters.
- `demo/recorder/recorder_control.c` invokes `midterm_boot_init/poll/link_error`, hard-enables vision route, and latches export STOP until reset. It cannot substitute unchanged for the C26 controller. The joint candidate needs a private Control/Vision adapter preserving current native C26 semantics, including track Bayes route hints.
- Existing `recorder_capture.c` separates Capture(4), Vision(3) and bounded recording queues; Writer(2) owns FatFs. Control(5) must remain highest task priority, call PI independently of camera publications, and retain PIT1 10ms safety sampling. CSI IRQ must never invoke task APIs at priority2.
- Reuse audited SD adapters and initialize PLL/PFD once before Camera and control. No runtime clock retune. Do not synchronously call FatFs from Control or the bare-metal foreground loop.
- Force motor1/motor2/servo duties to zero in both PWM init and set paths, irrespective of logical PI requests. Recorder enable must neither arm nor inhibit C26; recording OFF must retain Camera/Vision/control. Export only after freeze/close; keep every file and unique CREATE_NEW session.
- Same firmware configuration must compare finite Recorder OFF / ON / OFF windows, with bounded capture frames/bytes, phase-tagged counters, queue depth/drop, Encoder sample intervals, PI consumed sequence/work duration, control release/service lateness, IRQ mask, task stack margins and actual zero PWM registers. Reset per-window metrics at explicit phase boundaries; distinguish max service gap from a defined deadline miss counter.
- A stationary no-count protection trip is expected if C26 logical drive is exercised with wheels stopped. Report fault and limited PI-active duration; never bypass Encoder protection or label an idle PI window as sustained closed-loop validation.
- Prior B2-only and B3-only reports do not establish combined schedulability. Pending real candidate build, MAP proof, host handoff tests, manual flash and combined hardware windows.

No production project/source or firmware outputs were edited by this measurement step. No physical speed profile or CPR hypothesis was installed.
