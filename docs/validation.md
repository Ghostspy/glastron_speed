# Validation - 2026-11-21

Target: Waveshare ESP32-S3-Touch-LCD-2.8C, ESP32-S3, 16 MB flash, 8 MB PSRAM.
SDK: ESP-IDF 5.1.5; PlatformIO Espressif 32 6.5.0; LVGL 8.3.11.

## Verified in this workspace

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

## Local tool environment

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
