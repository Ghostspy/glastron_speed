# Glastron SSV-175 GPS speedometer + fuel

Companion to the adjacent `../tach` project, for the Glastron SSV-175 with the
1984 Mercury 115 HP. Firmware targets the **Waveshare ESP32-S3-Touch-LCD-2.8C**,
round 480 x 480, with ESP-IDF 5.1.5 and vendored LVGL 8.3.11.
The source tree is independent of the tach project.

![Speedometer demo](previews/speed-normal.png)

## Current behavior

- Matching black face, blue 270-degree **0-60 MPH** dial, large speed, fuel percent
  and bar. The dial endpoint is a display choice, not a boat speed specification.
- The same official Glastron logo and SSV-175 ten-second startup as the tach.
- Tap the face for day/night brightness (100% / 20%) and blue/amber accent.
- `speed` build: explicit **DEMO** speed/fuel, with stopped, idle/no-wake,
  acceleration, cruise, low/empty fuel, and missing-data examples every 40 seconds.
- `speed-gps` build: real UART GPS speed from checksum-checked NMEA RMC.
  It displays **GPS** with a valid fresh speed, **GPS UNAVAILABLE** otherwise.
  Fuel shows `-- %` / `Fuel unavailable`; it is never simulated in GPS mode.
- Independent two-second expiration; invalid or stale readings become dashes and
  the related arc/bar clears. Valid zero means stopped or empty, not missing data.

**Fuel sender acquisition and marine power conditioning are not implemented.**
Fuel below 15% is a provisional presentation threshold. No tank volume, range,
engine RPM conversion, wireless service or GPS receiver configuration is assumed.
Host/firmware validation does not establish physical sensor accuracy.

## Build in PlatformIO / VS Code

Open this directory in VS Code with PlatformIO IDE (`platformio.platformio-ide`).
Select **speed** for the USB demo or **speed-gps** for actual GPS input.
The included Build, Upload and Monitor tasks use the selected environment.
The first build downloads the pinned SDK/toolchain. Generated assets and LVGL
sources are included; no additional graphics library installation is required.

```sh
pio run -e speed
pio run -e speed-gps
pio run -e speed-gps -t upload
pio device monitor -e speed-gps
```

Use **USB TO UART** to upload and monitor at 115200 baud. Ports are autodetected;
set `upload_port` and `monitor_port` if multiple devices are connected. Close the
monitor before another program uses that port. These steps do not require GPS.

Each environment has its own generated config (`sdkconfig.speed`,
`sdkconfig.speed-gps`) and build directory (`.pio/build/<environment>/`). The GPS
environment applies `sdkconfig.gps.defaults` after the common defaults. A build
guard rejects a saved provider choice that disagrees with the environment; remove
that environment's generated config and rebuild to restore its defaults. Under
**Boat speedometer** in menuconfig, set brightness and the receiver's baud rate:

```sh
pio run -e speed-gps -t menuconfig
```

The build's automatic PSRAM check verifies that LVGL's large arena is external,
preserving internal DMA memory. Do not add another LVGL `lib_deps` copy.
If the CLI fails importing `pkg_resources`, use PlatformIO's Python 3.11 environment
(as supplied by the VS Code extension). Some newer Homebrew Python environments
do not include that dependency required by the pinned Espressif platform.

## GPS receiver and wiring

### Recommended receiver

Use the **Adafruit Ultimate GPS Breakout with GLONASS + GPS - PA1616D**,
**product ID 5440**:

- [Manufacturer product page / purchase link](https://www.adafruit.com/product/5440)
- [Manufacturer pinout and wiring guide](https://learn.adafruit.com/adafruit-ultimate-gps/pinouts)

This assembled breakout provides NMEA serial output at **9600 baud by default**,
ESP32-compatible **3 V UART output**, and a **3.3-5 V VIN** supply input. Its
GPS/GLONASS output can use the `GN` talker prefix, which this firmware supports.
Use the existing `speed-gps` build with `SPEED_GPS_BAUD=9600` and confirm RMC output
at least once per second; the firmware does not send receiver configuration commands.

The built-in patch antenna is suitable when it has a clear sky view. For mounting
under the Glastron's dash, use its **u.FL connector for an external 3 V active GPS
antenna**, adding a u.FL-to-SMA adapter if required by the antenna connector.
Mount the antenna where it can see the sky and protect the breakout and connections
from water. This is a component recommendation; this receiver has not yet been
tested with the physical gauge. Specifications checked on 2026-11-21; check current
availability before ordering.

### Connections

Use a receiver/module that emits **NMEA RMC** (`GPRMC`, `GNRMC`, or another uppercase
two-letter talker) through a **3.3 V logic UART**, **9600 baud, 8N1** by default.
Match `SPEED_GPS_BAUD` to the actual receiver if different. Configure the receiver
to output RMC at least once per second; sentence gaps over two seconds expire speed.
The table below applies to the recommended Adafruit #5440 breakout.

| Receiver connection | Display connection |
|---|---|
| TX, 3 V logic output | External UART **RXD**, ESP32 **GPIO44** |
| GND | GND |
| RX | Leave unconnected; firmware sends no commands |
| VIN | Clean regulated **5 V** supply; never the raw boat 12 V feed |

Connect power to **VIN**, not the breakout's **3.3V regulator-output** pin.
Keep receiver and display grounds common. Other receiver models need their own
supply/interface check before using this wiring.

The board's 4-pin UART header and 12-pin interface expose the same switched UART
signals. Follow **signal labels**, not guessed connector order or wire colors.
Do not connect RS-232, differential NMEA 0183, or 5 V UART signals directly to RXD;
those require the appropriate receiver/level interface. A 5 V powered GPS breakout
does not necessarily have a 3.3 V TX signal.

**Board-specific USB switch:** plugging in **USB TO UART disables the external
UART header**. To run with GPS, upload first, unplug USB TO UART, and power the
board through its **native USB** port or a verified regulated supply. Keep USB TO
UART disconnected while receiving GPS. Serial monitoring through that port will
temporarily disconnect the GPS input; the UI then expires the speed reading.
UART1 receives GPS on GPIO44; UART0 retains the 115200-baud diagnostic console.
Do not connect the display TXD to GPS RX: console logs are not receiver commands.

This switch behavior and pin map are documented in the
[official Waveshare board interfaces](https://docs.waveshare.com/ESP32-S3-Touch-LCD-2.8C).

Mount the antenna with sky visibility. A connected receiver without a valid fix
must display unavailable speed. GPS measures **speed over ground**, so water
current can make it differ from a pressure or paddlewheel speedometer.

## GPS data handling

`gps_nmea.c` accepts complete RMC sentences with a mandatory XOR checksum and
strict nonnegative decimal knots, converting to MPH by 1.150779448. Active status
`A` and measured modes `A`, `D`, `P`, `R`, `F` are accepted; legacy RMC without a
mode field is supported. Void status, bad speed, or non-measured/unknown mode
clears speed immediately. In particular, simulated/manual/estimated positions
cannot appear as measured speed. Navigation-status `V` at the end of modern RMC
is distinct from the data-validity status and does not itself void the fix.

Bad checksums, incomplete/overlong lines, unsupported sentence types and stream
noise never refresh sample age. `$` resynchronizes the stream. The parser has a
160-byte fixed buffer, no dynamic allocation and no third-party GPS dependency.
Partial sentences expire after two seconds; embedded carriage returns and extra
RMC fields are rejected. Input buffered across a service gap over two seconds is
discarded so old fixes cannot acquire a fresh timestamp on task resumption.
The UART shell handles at most 256 bytes and 16 events each loop, polls during
startup, and discards and invalidates data after UART overflow or framing errors.
Inputs above 200 MPH are rejected as a software sanity bound; readings above
60 MPH remain numeric while the display arc stops at its endpoint.

Protocol references: [u-blox RMC description](https://content.u-blox.com/sites/default/files/products/documents/u-blox8-M8_ReceiverDescrProtSpec_UBX-13003221.pdf),
[ESP-IDF 5.1.5 UART API](https://docs.espressif.com/projects/esp-idf/en/v5.1.5/esp32s3/api-reference/peripherals/uart.html).

## Host tests and previews

Requires CMake, a C11 compiler, Ninja and Python 3 (for build-source guard tests).
Pillow is only needed to regenerate fonts/logo or convert PPM images; normal
firmware builds use included C assets.
Helper dependencies are pinned in `requirements.txt` (Python 3.11 or newer).

```sh
sh scripts/test-model.sh
cmake -S tests -B build-host -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host
ctest --test-dir build-host --output-on-failure
build-host/render-ui gps preview.ppm
```

The model and GPS tests run under address/undefined-behavior sanitizers via the
shell script. The CMake suite also checks startup timing and renders the **actual
firmware C/LVGL face**, including value changes, expiry, zero and recovery.
It exercises the real UART provider against a small host IDF boundary, including
buffer discard after service gaps, UART errors and recovery. Hardware UART timing
and reception still require a board.
Scenes: `normal`, `low`, `empty`, `missing`, `speed-missing`, `fuel-missing`, `gps`,
`gps-waiting`, `stopped`, `night`, `startup`. PNG examples are in `previews/`.
Gray corners mark pixels outside the physical circular panel.

## Source map and fuel integration

| File | Responsibility |
|---|---|
| `main/board.c`, `panel_init.h` | Inherited board pin map, LCD, touch and brightness |
| `main/main.c` | Watchdog/event loop and explicit provider selection |
| `main/speed_model.c` | Independent timestamped readings and demo scenarios |
| `main/gps_nmea.c` | Host-testable streaming RMC parser |
| `main/gps_uart.c` | Bounded ESP-IDF UART1 acquisition |
| `main/speed_ui.c` | Sensor-independent LVGL face |
| `main/speed_startup.c` | Nonblocking visible-time Glastron startup |

When the protected, calibrated fuel interface is ready, publish a percent through
`speed_model_set_fuel(&model, percent, now_ms)` at the hook in `main.c`.
Stop publishing on failed acquisition, or publish `NAN` to invalidate immediately.
Use a FreeRTOS queue if another task acquires readings; keep model and LVGL updates
on the application task. No sender resistance, ADC pin, tank size or curve is
assumed. Never connect a boat sender or 12 V feed directly to this board.

## Standalone ESP-IDF

With ESP-IDF 5.1.5 activated:

```sh
idf.py build
idf.py -B build-gps -DSDKCONFIG=sdkconfig.gps -DBOAT_GPS_BUILD=ON build
```

The first command builds the demo; the second uses separate GPS config/output.
PlatformIO produces `firmware.elf`; standalone IDF produces `boat_speed.elf`.
`scripts/check-memory.py ELF --objdump TOOL` accepts either. The inherited
`scripts/package-firmware.py` packages demo builds only (standalone ELF name by
default); it refuses to label live GPS firmware as a demo. No prebuilt release
or hardware flash is implied by the source being present.

## Hardware acceptance remaining

Verify actual boot/colors/orientation, ten visible seconds of logo, touch and
brightness, extended operation and helm readability. Then test receiver supply,
UART voltage/baud, cold-start acquisition outdoors, stopped/moving accuracy,
loss of antenna/fix, unplug/reconnect and USB-mux behavior. Compare GPS speed to
an independent reference. Fuel needs sender measurements/calibration. A permanent
boat installation also needs protected regulated power and suitable mounting.

See `docs/validation.md` for checks actually run and `THIRD_PARTY.md` for provenance.
