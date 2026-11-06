# Recorder Host tests

Run from the repository root. Select only the relevant suite during development:

```powershell
./tools/testing/Invoke-TestPlan.ps1 -Suite recorder -Compiler gcc
./tools/testing/Invoke-TestPlan.ps1 -Suite recorder-tools -Compiler gcc
./tools/testing/Invoke-TestPlan.ps1 -Suite recorder-startup -Compiler gcc
./tools/testing/Invoke-TestPlan.ps1 -Suite recorder-sd-pins -Compiler gcc
```

`recorder` runs 17 synthetic/mock cases, compiling three Host executables once per
suite. It covers actual C capture ownership, Storage task and SD OSA code, plus the
independent Python SCRAW consumer. The compiler prerequisite is explicit: GCC on
local supplemental runs, MSVC and the selected optimized/ASan mode on the existing
Windows Host CI matrix. The suite inherits the initialized compiler environment;
it never silently substitutes GCC for MSVC. Missing compiler/compile/runtime errors
fail the suite. Compile subprocesses have 60s limits, harness runs 30s, and the
shared runner enforces a 180s suite deadline with process-tree cleanup.

`recorder-tools` performs three isolated generation contracts: unchanged bytes keep
mtimes, repeated preparation keeps all output bytes/mtimes, and motion-review mode
changes only the necessary metadata/header/project, and peripheral initialization
precedes the first kernel object. It never runs Keil or a C
compiler. Its suite deadline is 60s.

`recorder-startup` executes the real Control/Link init/poll, vision, startup and PI
with mocked peripherals and a one-slot queue. It covers the default review lock
and 22 reviewed scenarios: 1s startup, corrupted RX qualification reset, repeated
errors, STOP, safe fragmented queries, export, record-only stop, invalid/duplicate
frames, camera loss, damaged commands, storage isolation, busy radio, transient
and persistent visual loss, near-only pixels, STOP/export during recovery. Kernel
preemption and concurrent SD throughput are not simulated. Board adapter tests
separately validate the NXP DATA error tags and clean STOP preservation.

`recorder-sd-pins` runs the actual private native 1-bit pin initializer and pad
callback against a mock that permits only CMD/CLK/DAT0 and card detect/power.
It rejects UART8 DATA2/3 and servo VSELECT writes. The generation contracts also
check actual host/card width negotiation, not merely capability header macros.

All four suites are required members of `Host` and `PreFlash`. Existing Host CI invokes
them through that profile; adding them does not schedule an automatic local build
or run a firmware build from the recorder suite. The runner now snapshots
`demo/recorder` inputs, so changes during execution invalidate its evidence.

Standalone development remains available:

```powershell
python -m unittest discover -s tests/recorder -v
```

Standalone defaults to GCC optimized; use the shared entrypoint for MSVC/ASan and
structured reports. Fixtures use temporary directories, with data roots outside
the Git repository. No real SD card, serial port, debugger or vehicle is accessed.
Passing Host tests does not verify physical DMA, task deadlines or driving stability.
