idf_component_register(
    SRCS "main.c" "board.c" "speed_model.c" "speed_ui.c" "speed_font.c"
         "speed_startup.c" "glastron_logo.c" "gps_nmea.c" "gps_uart.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_lcd esp_timer esp_system lvgl)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
