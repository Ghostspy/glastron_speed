/* Minimal IDF boundary for exercising the real UART provider on the host. */
#ifndef TEST_IDF_H
#define TEST_IDF_H
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
typedef void *QueueHandle_t;
typedef struct { int baud_rate, data_bits, parity, stop_bits, flow_ctrl, source_clk; } uart_config_t;
typedef struct { int type; } uart_event_t;
enum { ESP_OK, UART_NUM_1, UART_DATA_8_BITS, UART_PARITY_DISABLE,
       UART_STOP_BITS_1, UART_HW_FLOWCTRL_DISABLE, UART_SCLK_DEFAULT,
       UART_FIFO_OVF, UART_BUFFER_FULL, UART_PARITY_ERR, UART_FRAME_ERR, UART_BREAK };
#define UART_PIN_NO_CHANGE (-1)
#define CONFIG_SPEED_GPS_BAUD 9600
#ifndef CONFIG_SPEED_GPS_AUTOCONFIG
#define CONFIG_SPEED_GPS_AUTOCONFIG 1
#endif
#define ESP_LOGI(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
esp_err_t uart_param_config(int port, const uart_config_t *config);
esp_err_t uart_set_pin(int port, int tx, int rx, int rts, int cts);
esp_err_t uart_driver_install(int port, int rx, int tx, int count, QueueHandle_t *queue, int flags);
esp_err_t uart_flush_input(int port);
esp_err_t uart_set_baudrate(int port, uint32_t baud);
int uart_tx_chars(int port, const char *buffer, uint32_t size);
int uart_read_bytes(int port, void *buffer, unsigned size, unsigned timeout);
int xQueueReceive(QueueHandle_t queue, void *event, unsigned timeout);
int xQueueReset(QueueHandle_t queue);
#endif
