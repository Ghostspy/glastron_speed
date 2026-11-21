# GPS Autoconfiguration Implementation Plan

**Goal:** Detect and configure the user's u-blox-compatible GPS at startup.

**Architecture:** A host-testable UBX state machine produces one small UART action
per poll. The existing UART provider handles pins, receive demultiplexing and errors.

**Tech Stack:** C11, ESP-IDF 5.1.5, PlatformIO, existing CMake host/sanitizer tests.

**Spec:** `docs/superpowers/specs/2026-11-21-gps-autoconfiguration-design.md`

## Global constraints

Fixed buffers; bounded nonblocking operations; no EEPROM writes; 8N1;
GPIO43/44; native USB console for automatic configuration; generic NMEA fallback.

## Review focus

Wrong-baud and binary-only receiver settings; damaged/oversized UBX packets;
unrelated ACK and NAK; port-change acknowledgment lost during baud transition;
missing return wire and existing receive-only receiver behavior.

### Task 1: Protocol and startup state machine

- [x] Add failing host tests for discovery, configuration/readback, corruption,
  timeouts, NAK, fallback and binary/NMEA separation; run and confirm failure.
- [x] Implement `gps_ublox_init`, `gps_ublox_feed`, `gps_ublox_next` in
  `main/gps_ublox.{c,h}`. Actions contain an optional baud change and UBX packet.
- [x] Run protocol tests under address/undefined-behavior sanitizers.

### Task 2: UART integration and configuration

- [x] Extend real-provider boundary tests for TX GPIO43, baud switches, errors,
  valid GPS recovery, and auto-configuration disabled.
- [x] Connect protocol actions to nonblocking UART TX, add Kconfig switch and
  USB console defaults with a persisted-config guard; wire both build systems.
- [x] Run all host tests, sanitizer tests and demo/GPS firmware builds.

### Task 3: Documentation and final verification

- [x] Update README wiring, receiver setup, USB monitoring, limitations and
  acceptance checklist; record actual verification in `docs/validation.md`.
- [x] Review the complete diff, correct issues, and rerun affected checks.

Execution: inline in the user-supplied workspace, with changes left reviewable.

Completed: 18/18 host tests, sanitizer checks and both PlatformIO builds pass.
Independent review found no further important issues. Additional regressions
cover continuous framing errors, driver failures, and serialized low-baud replies.
Physical validation remains as listed in `docs/validation.md`.
