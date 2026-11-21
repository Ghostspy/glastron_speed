cmake_minimum_required(VERSION 3.16)
# This display uses an ESP32-S3. Native IDF otherwise defaults to ESP32 before
# loading sdkconfig.defaults, which is too late to select the toolchain.
set(IDF_TARGET "esp32s3" CACHE STRING "Target MCU")
set(PROJECT_VER "0.1.0")
if(BOAT_GPS_BUILD)
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.gps.defaults")
endif()
set(COMPONENTS main)
# PlatformIO needs its generated helper component to obtain C/C++ build settings.
if(BOAT_PLATFORMIO)
    list(APPEND COMPONENTS __pio_env)
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(boat_speed)

if(CONFIG_SPEED_GPS_AUTOCONFIG AND CONFIG_ESP_CONSOLE_UART)
    message(FATAL_ERROR "GPS autoconfiguration uses GPIO43: select USB Serial/JTAG console in menuconfig, or regenerate the GPS sdkconfig to apply sdkconfig.gps.defaults.")
endif()

# Defaults seed new configs only. A persisted menuconfig choice must not turn a
# named GPS build into a demo image (or a demo build into a hardware reader).
if(BOAT_PLATFORMIO)
    if(BOAT_GPS_BUILD AND NOT CONFIG_SPEED_GPS_MODE)
        message(FATAL_ERROR "speed-gps requires GPS mode. Remove sdkconfig.speed-gps and rebuild.")
    elseif(NOT BOAT_GPS_BUILD AND NOT CONFIG_SPEED_DEMO_MODE)
        message(FATAL_ERROR "speed requires DEMO mode. Remove sdkconfig.speed and rebuild, or use speed-gps.")
    endif()
endif()
