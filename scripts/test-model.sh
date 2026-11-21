#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build-host
for unit in model gps; do
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic \
        -fsanitize=address,undefined -g -I main \
        "tests/test_${unit}.c" main/speed_model.c main/gps_nmea.c \
        -lm -o "build-host/test-${unit}-sanitized"
    "./build-host/test-${unit}-sanitized"
done
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -g -I main \
    tests/test_ublox.c main/gps_ublox.c -o build-host/test-ublox-sanitized
./build-host/test-ublox-sanitized
for auto in 0 1; do
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -g -I main -I tests/fake_idf -DCONFIG_SPEED_GPS_AUTOCONFIG=$auto \
    tests/test_uart.c main/gps_uart.c main/gps_nmea.c main/gps_ublox.c main/speed_model.c \
    -lm -o build-host/test-uart-sanitized
./build-host/test-uart-sanitized
done
