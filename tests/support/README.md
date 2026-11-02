# Shared Host support

Fixtures under `fixtures/` own reusable clock, PWM, encoder, camera, UART and
route/switch models. A fixture contains no test entrypoint or scenario assertions
from another test. Each executable owns its own model state and explicit reset.

The existing platform stubs remain in `tests/vision/stubs/`: they are deliberately
not moved, preserving include ordering and compiler behavior. The historical
vision algorithm stays in `tests/vision/reference/`, linked as a separately renamed
object, never included in the target firmware.

Safety assertions in shared IO adapters remain active. Sharing a fixture never
means inheriting another executable's initialization or prior run.
