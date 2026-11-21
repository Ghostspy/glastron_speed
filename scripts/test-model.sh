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
