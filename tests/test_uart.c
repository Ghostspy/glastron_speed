#include "gps_uart.h"
#include "test_idf.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static char buffered[4096];
static size_t buffered_size;
static unsigned flushes, read_calls;
static int error_event;
static const char *fix = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";

esp_err_t uart_param_config(int port, const uart_config_t *config)
{ assert(port == UART_NUM_1 && config->baud_rate == 9600); return ESP_OK; }
esp_err_t uart_set_pin(int port, int tx, int rx, int rts, int cts)
{ (void)rts; (void)cts; assert(port == UART_NUM_1 && tx == -1 && rx == 44); return ESP_OK; }
esp_err_t uart_driver_install(int port, int rx, int tx, int count, QueueHandle_t *queue, int flags)
{ (void)flags; assert(port == UART_NUM_1 && rx == 2048 && tx == 0 && count == 16); *queue = buffered; return ESP_OK; }
esp_err_t uart_flush_input(int port)
{ assert(port == UART_NUM_1); buffered_size = 0; ++flushes; return ESP_OK; }
int xQueueReceive(QueueHandle_t queue, void *event, unsigned timeout)
{
    (void)queue; assert(timeout == 0);
    if (!error_event) return 0;
    ((uart_event_t *)event)->type = error_event; error_event = 0; return 1;
}
int xQueueReset(QueueHandle_t queue) { (void)queue; error_event = 0; return 1; }
int uart_read_bytes(int port, void *data, unsigned size, unsigned timeout)
{
    assert(port == UART_NUM_1 && size <= 256 && timeout == 0);
    ++read_calls;
    if (size > buffered_size) size = (unsigned)buffered_size;
    memcpy(data, buffered, size);
    buffered_size -= size;
    memmove(buffered, buffered + size, buffered_size);
    return (int)size;
}
static void receive(const char *data)
{
    buffered_size = strlen(data);
    assert(buffered_size < sizeof buffered);
    memcpy(buffered, data, buffered_size);
}

int main(void)
{
    speed_model_t model = {0};
    assert(gps_uart_init() == ESP_OK);
    receive(fix);
    gps_uart_poll(&model, 0);
    assert(model.speed_mph.valid && !model.demo && !model.fuel_percent.valid);
    receive(fix); /* Already queued before the application stalls. */
    gps_uart_poll(&model, 3000);
    assert(flushes == 1 && !model.speed_mph.valid && buffered_size == 0);
    receive(fix);
    gps_uart_poll(&model, 3010);
    assert(model.speed_mph.valid && model.speed_mph.updated_ms == 3010);
    receive(fix);
    error_event = UART_FIFO_OVF;
    gps_uart_poll(&model, 3020);
    assert(flushes == 2 && !model.speed_mph.valid);
    receive(fix);
    gps_uart_poll(&model, 3030);
    assert(model.speed_mph.valid);
    memset(buffered, 'x', sizeof buffered);
    buffered_size = sizeof buffered;
    unsigned before = read_calls;
    gps_uart_poll(&model, 3040);
    assert(read_calls == before + 1 && buffered_size == sizeof buffered - 256);
    puts("PASS: UART pin/baud, bounded service, stalled input discard, overflow and recovery");
}
