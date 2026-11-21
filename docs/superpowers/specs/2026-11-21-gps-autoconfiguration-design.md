# GPS automatic configuration

The requested change lets an existing GY-NEO6MV2/NEO-6M recover from saved
flight-controller serial settings without a separate configuration computer.

Use checksum-validated UBX MON-VER replies to detect a u-blox-compatible receiver;
the carrier board's model cannot be identified over UART. Probe the configured
baud first, then common 4800–230400 baud rates, using 8N1. Configure UART1 for the
selected baud (9600 by default), UBX/NMEA input/output, one-second navigation and
RMC every solution. Check acknowledgments and read back port/rate/message settings.
Apply settings in RAM each boot, without EEPROM writes or factory resets.

Alternatives: receive-only operation cannot repair saved settings; broadcasting
configuration before identification can alter unrelated receivers. Use read-only
probes until MON-VER confirms the protocol. Keep an option to disable automatic
configuration for receive-only installations. After bounded failure, retain
ordinary NMEA reception at the configured baud; restart to repeat detection.

Use GPIO43 TX and GPIO44 RX. GPS builds move the application and bootloader
console to native USB Serial/JTAG to keep logs out of GPS RX. ROM reset output
may still appear on GPIO43; the receiver must tolerate startup noise. USB TO UART
must be disconnected for the board's external UART switch to pass either signal.

The state machine uses fixed storage and bounded, nonblocking service during the
existing startup animation. Configuration traffic must never become a speed fix.
Corrupt packets, missing ACKs and disconnected RX fail without claiming success.
Preserve invalid/stale-speed clearing, watchdog service and the demo build.
Hardware acceptance still requires the actual display and receiver.
