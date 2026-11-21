# Development record

Plan: `docs/superpowers/plans/2026-11-21-speedometer.md`.

- User approved the matching speed/fuel start and requested GPS speed.
- Work in the requested empty `speed` repository; no prior commits or source tests
  exist. Keep changes here for review. A linked worktree has no base commit yet.
- Execute locally in this session; user has authorized implementation.
- Ruling: use receiver-independent RMC input instead of selecting an unrequested
  GPS module. Module supply and antenna selection remain hardware decisions.
- Preflight: model/parser feed both UI and UART shell through the same timestamped
  setters. Demo and GPS configurations must never mix providers.
- Task 1: complete. Tests failed before model/parser source existed, then passed
  under AddressSanitizer and UndefinedBehaviorSanitizer. Includes fixed-seed noisy
  byte-stream coverage and recovery, legacy/modern RMC and explicit fix loss.
- Task 2: host UI tests failed before the UI source existed, then all 14 CMake
  checks passed. Normal, GPS and night PNGs visually inspected.
- Firmware tool issue: Homebrew PlatformIO runs Python 3.14 without pkg_resources;
  the existing PlatformIO Python 3.11 environment imports it successfully. Builds
  use that environment; no global packages changed. Local ignored package cache
  was copied from tach, so no toolchain changes were made in the tach project.
- A First commit (747570d) appeared while implementation was in progress; it
  contains the initial foundation. Continue preserving all current work in place.
- Task 2: complete. Both named firmware builds succeed; PSRAM checks pass and
  generated configs confirm strict demo/GPS provider selection.
- Final review: independent reviewer identified provider persistence and stale
  serial input issues. Added source-guard, delayed-LF, malformed-layout and UART
  suspension regressions; watched the failing cases before applying fixes.
  All 16 host checks and three sanitized executables now pass. No deferred minors.
- Ruling: GPS module selection remains open; receive 3.3 V NMEA RMC on the board's
  switched UART header. Cost if a future receiver uses a different electrical or
  sentence interface: add its appropriate interface before connecting it.
- Task 3: complete. Build/wiring/fuel integration instructions and measured
  validation limits are recorded. No flash, external publication or source push.
