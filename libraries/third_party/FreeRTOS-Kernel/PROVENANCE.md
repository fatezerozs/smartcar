# FreeRTOS Kernel subset

Official upstream: https://github.com/FreeRTOS/FreeRTOS-Kernel

Version V11.1.0, peeled commit `dbf70559b27d39c1fdb68dfb9a32140b6a6777a0`.
MIT license: [LICENSE.md](LICENSE.md). Retrieved 2026-11-04.

Unmodified tasks.c, queue.c, list.c, include/ (except the unused deprecated
StackMacros.h forwarding header, omitted to avoid case-insensitive Windows
checkout collisions with stack_macros.h) and portable/GCC/ARM_CM7/r0p1/.
Only the dedicated recorder training target links this subset. No heap allocator,
software timer daemon or replacement of the competition target is included.
The Cortex-M7 r0p1 port contains the erratum 837070 workaround; actual CPU/FPU
exception switching and interrupt latency remain target acceptance items.
