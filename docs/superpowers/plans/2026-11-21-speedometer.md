# GPS Speedometer Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement task-by-task.

**Goal:** Build the tach's companion speed + fuel display with real UART GPS input.

**Architecture:** Pure C model and streaming NMEA parser feed the LVGL display from
one ESP-IDF application task. Demo and GPS builds are explicit and isolated.

**Tech Stack:** ESP-IDF 5.1.5, LVGL 8.3.11, C11, PlatformIO, CMake host tests.

**Spec:** `docs/superpowers/specs/2026-11-21-speedometer-design.md`

## Global Constraints

- Waveshare ESP32-S3-Touch-LCD-2.8C (480 x 480).
- ESP-IDF 5.1.5 and LVGL 8.3.11.
- Speed and fuel expire independently after 2000 ms.
- UART1 RX on GPIO44 (board UART RXD), 9600 baud, 8N1, configurable baud.
- No changes to `../tach`, no simulated fuel in GPS mode, no automatic fallback.
- All LVGL calls occur on the application task; no parser heap allocation.

## Review Focus

- Bad checksum/noise must not keep old GPS speed alive.
- Void, simulated, estimated or malformed RMC must not display measured speed.
- Overlong/fragmented input must recover without corrupting memory.
- GPS mode must never display demo fuel or speed, including startup.
- UART pin selection and USB mux must match the board, with bounded service time.

### Task 1: Independent model and NMEA ingestion

**Files:** `main/speed_model.{h,c}`, `main/gps_nmea.{h,c}`,
`tests/test_model.c`, `tests/test_gps.c`, `scripts/test-model.sh`.

**Interfaces:** `speed_model_set_speed(model, mph, now_ms)`,
`speed_model_set_fuel(model, percent, now_ms)`, `speed_model_view(model, now_ms)`,
`gps_nmea_feed(parser, byte, model, now_ms)`; timestamps are uint64_t milliseconds.

- [ ] Write model and parser tests for the spec's normal and adverse cases.
- [ ] Compile/run before implementation; expect missing symbols/files.
- [ ] Implement the model and streaming checksum-checked parser.
- [ ] Run `sh scripts/test-model.sh`; expect sanitized model and GPS tests to pass.

### Task 2: Matching display and firmware GPS provider

**Files:** Reused board, LVGL and asset sources; `main/speed_ui.{h,c}`,
`main/speed_startup.{h,c}`, `main/gps_uart.{h,c}`, `main/main.c`,
`main/Kconfig.projbuild`, `platformio.ini`, config defaults, host renderer/tests.

**Interfaces:** Task 1 model and parser APIs; `gps_uart_init()`,
`gps_uart_poll(model, now_ms)`, `speed_ui_create/update/set_night`.

- [ ] Adapt renderer assertions for speed/fuel, stale/recovery and GPS statuses;
  run before the matching UI exists and confirm failure.
- [ ] Reuse the tach board/assets/startup and implement the matching UI.
- [ ] Add bounded UART1 ingestion and explicit demo/GPS configurations.
- [ ] Run CMake host suite, render PNG previews and inspect them.
- [ ] Build `speed` and `speed-gps`; check automatic PSRAM placement report.

### Task 3: Documentation and final validation

**Files:** `README.md`, `THIRD_PARTY.md`, `docs/validation.md`, previews.

- [ ] Document builds, receiver requirements, header wiring, USB mux behavior,
  sample expiry, parser limits, fuel integration hook and hardware acceptance.
- [ ] Run the full host suite, sanitized tests and both firmware builds.
- [ ] Review source/configuration for GPS/demo isolation and board conflicts.
- [ ] Report verified results and remaining hardware work; leave changes reviewable.
