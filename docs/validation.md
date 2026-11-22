# Validation - 2026-11-21

Target: Waveshare ESP32-S3-Touch-LCD-2.8C, ESP32-S3, 16 MB flash, 8 MB PSRAM.
Current SDK: ESP-IDF 5.4.4; PlatformIO Espressif 32 6.11.0; LVGL 8.3.11.

## GPS course display

- Added eight-point compass direction and rounded true course above the digital
  speed (`NE • 045°T`), with blue/day and amber/night accents. Moved the branding
  upward to retain the existing speed size and position.
- RMC course is validated independently of speed. Missing/malformed course clears
  direction while preserving valid speed; invalid fixes and UART failures clear
  both. Course expires after two seconds and is suppressed below an unrounded
  speed of 2 MPH. This cutoff is an initial display policy, not measured accuracy.
- North remains valid at 0°, and a rounded 360° wraps to 000°. The explicitly
  labelled demo includes simulated course; GPS mode uses receiver data only.
- Host suite: **21/21 pass**. Added course bounds, missing/invalid data, independent
  expiration, low-speed suppression, compass sector boundaries, north wrap,
  display transitions, font glyph availability and placement above digital speed.
- `sh scripts/test-model.sh`: all model/parser/UBX/UART checks pass with address
  and undefined-behavior sanitizers, including receive-only and automatic setup.
- Actual C/LVGL day, night, GPS and low-speed previews were visually inspected;
  branding, course and speed do not overlap. Updated PNGs are in `previews/`.
- Both `speed` and `speed-gps` firmware builds pass with ESP-IDF 5.4.4 using
  VS Code's bundled PlatformIO Core, including both PSRAM placement checks.
  Build log: `/tmp/boat-course-firmware-builds.log`. No board was flashed.

## ESP-IDF 5.4.4 migration

- Reproduced the incomplete `/download/v5.4.4/...` package URL failure. The
  official Espressif release ZIP has no PlatformIO manifest; pointing PlatformIO
  directly at it incorrectly selects a nested cJSON test package. The local
  `scripts/espidf` adapter supplies framework metadata and installs the official
  SDK after checking its published SHA-256. It caches the complete release ZIP
  and extracts components, tools and root configuration files.
- Pinned GCC 14.2.0 `esp-14.2.0_20260121` for Xtensa and RISC-V, as required by
  the 5.4.4 SDK. Updated the post-link objdump path for the unified Xtensa tools.
- Replaced the deprecated RGB LCD `psram_trans_align` initializer with
  `dma_burst_size`; application compilation retains `-Werror`.
- Corrected the standalone build from `esp32` to `esp32s3`, with an early CMake
  target default. Preserved the previous native sdkconfigs in temporary backups
  before regenerating them. Workspace settings disable the component manager
  because all required components are vendored, as in the PlatformIO workflow.
- Using VS Code's bundled PlatformIO Core 6.2.0:
  `~/.platformio/penv/bin/python -m platformio run -e speed -e speed-gps`:
  **both succeed**, including bootloader, partition and application images and
  both automatic PSRAM placement checks. GPS reports 19,600 bytes static internal
  RAM and 644,508 bytes application flash. This does not measure runtime margins.
- The native ESP-IDF 5.4.4 demo build succeeds and passes the standalone PSRAM
  placement check. This automation session runs under Rosetta; the native SDK's
  Python has ARM-only dependencies. Validation used a temporary ARM-only copy of
  that same Python executable. The generated CMake settings were then restored
  to the original installed interpreter for normal VS Code use.
- `cmake -S tests -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug`,
  `cmake --build build-host`, `ctest --test-dir build-host --output-on-failure`:
  **18/18 pass**, including provider/configuration guards and rendering tests.
- The official download URL, TLS verification with PlatformIO's CA bundle, ZIP
  header and full cached release checksum were exercised. No board was flashed.

Build logs: `/tmp/boat-vscode-idf544-builds.log`,
`/tmp/boat-native-idf544-build.log`, `/tmp/boat-host-idf544-tests.log`.

## Original ESP-IDF 5.1.5 baseline

The following records the earlier 5.1.5 / Espressif 32 6.5.0 validation.

### Verified in this workspace

- `sh scripts/test-model.sh`: model, GPS parser, UBX setup and real UART provider pass with
  AddressSanitizer and UndefinedBehaviorSanitizer. Covers zero/empty, invalid and
  nonfinite values, independent two-second expiration, display/input bounds,
  long uptime, demo states, published RMC conversion, checksum failures, fix loss,
  measured vs simulated modes, noise/overflow, split streams and recovery.
  UBX tests cover u-blox 6/7 version replies, alternate baud discovery,
  checksum/length rejection, ACK/NAK and readback, timeout/fallback, custom baud,
  and sufficient transmit drain time at 1200 baud. Provider tests run with
  automatic setup enabled and disabled, simulate a 38400-baud receiver being
  configured to 9600, and check binary/NMEA separation, short writes, driver
  failures and baud scanning despite continuous framing errors.
- Regression tests failed before their fixes for delayed sentence termination,
  malformed CR framing, persisted build/provider mismatch and UART backlog after
  a long application service gap; all pass after the fixes.
- `cmake -S tests -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug`,
  `cmake --build build-host`, and
  `ctest --test-dir build-host --output-on-failure`: **18/18 pass**.
  This includes provider guards, model, parser, UART shell, startup timing and
  eleven actual C/LVGL rendering scenes, UBX setup and receive-only operation.
  Build guards also reject a saved UART console when GPS TX uses GPIO43.
  The host UART boundary validates
  application behavior, not electrical UART timing.
- Demo and GPS PlatformIO builds both succeed and produce `firmware.bin`,
  bootloader and partition images in `.pio/build/speed/` and `.pio/build/speed-gps/`.
  The application image is not a combined offset-zero flash image.
- Actual generated JSON configs confirm `speed`: demo=true, GPS=false, UART0
  console; `speed-gps`: demo=false, GPS=true, autoconfig=true, native USB
  Serial/JTAG console; receiver baud defaults to 9600. Regeneration of the GPS
  config from the defaults was exercised, not just a manually edited config.
- Post-link PSRAM placement passes for both images. GPS image reports
  17,112 bytes static internal RAM (5.2%) and approximately 650 KB flash (20.7% of the
  application partition). This is not a runtime heap/stack margin measurement.
- Normal, GPS, night, low fuel, empty, missing, GPS waiting and startup PNGs were
  visually inspected. Labels and graphics fit the circular viewport.
- Independent source review completed. Build/provider agreement, stale partial
  GPS input and stalled UART backlog findings were corrected with regressions.
  Parser framing/layout strictness was also corrected. No deferred source findings.
- Working-tree and staged whitespace checks pass.

### Original local tool environment

The existing tach SDK/compiler cache was copied into the ignored project-local
`.pio/core` cache, retaining the pinned package versions. A fresh online download
of that entire toolchain was not tested. The adjacent tach source was not edited.

The Homebrew `pio` entry point uses Python 3.14 without `pkg_resources`, which the
pinned platform imports. The existing PlatformIO Python 3.11 environment provides
that dependency but lacked esptool dependencies. An isolated `.venv` was created
here and esptool 4.11.0/dependencies were installed with approved network access.
Successful firmware verification command (local paths, not portable setup):

```sh
PYTHONPATH=/Users/ghost/.platformio/penv/lib/python3.11/site-packages \
  .venv/bin/python -m platformio run -e speed -e speed-gps
```

No global Python installation was changed. The standard VS Code PlatformIO
environment or a clean Python 3.11 environment with the documented requirements
is the portable build route. Logs from this run are temporary files in
`/private/tmp/boat-speed-*-build.log`.

## Not verified on hardware

No board was flashed and no GPS receiver or boat wiring was connected. Still
required: boot, colors/orientation, touch, brightness, visible startup duration,
runtime stability/heap/stack, real UART/mux operation, receiver cold start and fix
loss, speed accuracy, antenna installation, sunlight readability, fuel sender
measurements/calibration and protected marine power/mounting.

GY-NEO6MV2/NEO-6M and GT-U7 are the user's available receiver candidates;
Adafruit PA1616D remains the documented alternative with automatic setup disabled.
No physical model was identified or exercised by the software tests. Verify both
UART directions, native USB logs, startup discovery, missing RX wire, configuration
ACK/readback, cold boot with changed saved baud/binary-only output, and power-cycle
behavior. Saved non-8N1 framing or UBX input disabled requires manual setup.
Fuel is simulated only in the demo and unavailable in the GPS build pending a
real calibrated provider.
