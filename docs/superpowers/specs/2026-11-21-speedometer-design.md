# Glastron speedometer with GPS

Approved direction: the user chose the matching speed + fuel starting point and
requested GPS for speed on 2026-11-21. This is the companion to `../tach` for the
Glastron SSV-175 with the 1984 Mercury 115 HP. The engine does not supply speed.

## Approach and scope

Reuse the tach's ESP-IDF 5.1.5, LVGL 8.3.11, Waveshare
ESP32-S3-Touch-LCD-2.8C (480 x 480), panel driver, generated font, official Glastron
logo, ten-second nonblocking startup, watchdog, and tap day/night behavior.
Retain a standalone source tree; do not modify or depend on the adjacent project.

The alternatives discussed were a display demo, GPS integration, and an existing
speedometer pickup. The user chose the matching display plus GPS integration.
Use receive-only 3.3 V UART NMEA RMC, independent of receiver brand. This avoids
assuming a pressure/paddlewheel interface or proprietary receiver commands.

Default `speed` build: clearly labeled simulated speed and fuel, USB bench demo.
Separate `speed-gps` build: actual GPS speed, no simulated fuel or demo fallback.
Fuel acquisition and boat power conditioning remain future work.

## Display

Black face, off-white text, blue progress arc (amber at night), Barlow Condensed
large whole MPH numerals, Montserrat labels, 270-degree 0-60 MPH dial, fuel percent
and bar below. Sixty MPH is a display endpoint, not a claimed boat capability.
Fuel below 15% has a provisional amber low-fuel indication; zero is valid empty.

```
              30
        20           40
     10     GLASTRON     50
             SSV-175
                32
      0        MPH       60
               68 %
              Fuel
        DEMO / GPS status
```

Speed and fuel expire independently after 2000 ms. Missing, invalid, future-dated,
and stale inputs render as dashes; arcs/bars clear. Valid zero remains zero.
Do not estimate fuel range or derive speed from RPM.

## GPS

UART1 RX on GPIO44 (board UART RXD), 9600 baud, 8N1, configurable baud.
Use the board's external UART header. Waveshare's UART selection chip disables
that header while USB TO UART is connected: power GPS operation through the native
USB port or a verified external supply, with USB TO UART unplugged. Flash and
monitor through USB TO UART separately. GPS TX must use 3.3 V logic, common ground;
GPS RX is unused. Confirm module supply requirements before connecting power.

Parse bounded streaming `$xxRMC` sentences with mandatory XOR checksum and strict
decimal speed in knots, converted by 1.150779448 MPH/knot. Accept active status A
with legacy missing mode or measured A/D/P/R/F mode; reject invalid, simulated,
estimated, manual, or unknown modes. Explicit void/invalid RMC clears speed
immediately; corrupt or unrelated packets never refresh the sample age. Only a
complete checksum-valid RMC updates the model. Bound reads per loop so a noisy
receiver cannot starve rendering/watchdog. No heap allocation in the parser.

## Validation and limits

Host tests cover sensor validity, independent expiration, zero, bounds, long uptime,
stream splitting, checksum errors, invalid fix, unsupported mode, line overflow,
noise and recovery. Render the actual LVGL C UI for normal, empty/low fuel,
no-data, partial-data, GPS waiting, stopped, night and startup scenes.
Build both firmware modes and check PSRAM placement. Hardware boot, GPS reception,
touch, colors, sunlight readability, fuel calibration and marine power validation
remain physical acceptance work. No firmware is flashed by this task.

## References checked 2026-11-21

- [Waveshare board interfaces and UART mux](https://docs.waveshare.com/ESP32-S3-Touch-LCD-2.8C)
- [ESP-IDF 5.1.5 UART API](https://docs.espressif.com/projects/esp-idf/en/v5.1.5/esp32s3/api-reference/peripherals/uart.html)
- [u-blox RMC protocol](https://content.u-blox.com/sites/default/files/products/documents/u-blox8-M8_ReceiverDescrProtSpec_UBX-13003221.pdf)
