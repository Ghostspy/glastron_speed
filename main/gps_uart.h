#ifndef GPS_UART_H
#define GPS_UART_H

#include "esp_err.h"
#include "speed_model.h"

esp_err_t gps_uart_init(void);
/* Nonblocking, bounded work. Call every loop, including during the logo. */
void gps_uart_poll(speed_model_t *model, uint64_t now_ms);

#endif
