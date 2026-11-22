#include "gps_uart.h"
#include "test_idf.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static char buffered[4096];
static size_t buffered_size;
static unsigned flushes, read_calls;
static int error_event;
static unsigned baud = 9600, writes;
static bool emulate, fail_baud, short_tx;
static unsigned module_baud;
static uint8_t module_port[20];
static void queue_frame(uint8_t cls, uint8_t id, const uint8_t *payload, size_t n)
{
    uint8_t wire[256] = {0xb5,0x62,cls,id,(uint8_t)n,0};
    assert(n+8 < sizeof wire && buffered_size+n+8 <= sizeof buffered);
    memcpy(wire+6,payload,n);
    uint8_t a=0,b=0;
    for (size_t i=2;i<n+6;++i) { a+=wire[i]; b+=a; }
    wire[n+6]=a; wire[n+7]=b;
    memcpy(buffered+buffered_size,wire,n+8); buffered_size+=n+8;
}
static const char *fix = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";

esp_err_t uart_param_config(int port, const uart_config_t *config)
{ assert(port == UART_NUM_1 && config->baud_rate == 9600); return ESP_OK; }
esp_err_t uart_set_pin(int port, int tx, int rx, int rts, int cts)
{ (void)rts; (void)cts;
  assert(port == UART_NUM_1 && tx == (CONFIG_SPEED_GPS_AUTOCONFIG ? 43 : -1) && rx == 44);
  return ESP_OK; }
esp_err_t uart_driver_install(int port, int rx, int tx, int count, QueueHandle_t *queue, int flags)
{ (void)flags; assert(port == UART_NUM_1 && rx == 2048 && tx == 0 && count == 16); *queue = buffered; return ESP_OK; }
esp_err_t uart_flush_input(int port)
{ assert(port == UART_NUM_1); buffered_size = 0; ++flushes; return ESP_OK; }
esp_err_t uart_set_baudrate(int port, uint32_t value)
{ assert(port == UART_NUM_1); if (fail_baud) return -1; baud = value; return ESP_OK; }
int uart_tx_chars(int port, const char *data, uint32_t size)
{
    assert(port == UART_NUM_1 && size <= 28 && (unsigned char)data[0] == 0xb5);
    ++writes;
    if (short_tx) return 0;
    if (emulate && baud == module_baud) {
        const uint8_t *p=(const uint8_t *)data+6;
        if (data[2] == 0x0a && data[3] == 4) {
            uint8_t ver[70]={0}; memcpy(ver,"7.03 (45969)",12); memcpy(ver+30,"00040007",8);
            queue_frame(0x0a,4,ver,sizeof ver);
        } else if (data[2] == 6 && data[3] == 0 && data[4] == 20) {
            memcpy(module_port,p,20);
            module_baud=(unsigned)p[8] | (unsigned)p[9]<<8 | (unsigned)p[10]<<16 | (unsigned)p[11]<<24;
        } else if (data[2] == 6 && data[3] == 0 && data[4] == 1) {
            queue_frame(6,0,module_port,20);
        } else if (data[2] == 6 && data[4] == 0 && data[3] == 8) {
            const uint8_t rate[]={0xe8,3,1,0,1,0}; queue_frame(6,8,rate,6);
        } else if (data[2] == 6 && data[4] == 2 && data[3] == 1) {
            const uint8_t rmc[]={0xf0,4,0,1,0,0,0,0}; queue_frame(6,1,rmc,8);
        } else {
            assert(data[2] == 6 && (data[3] == 1 || data[3] == 8));
            const uint8_t ack[]={6,(uint8_t)data[3]}; queue_frame(5,1,ack,2);
        }
    }
    return (int)size;
}
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
#if CONFIG_SPEED_GPS_AUTOCONFIG
    gps_uart_poll(&model, 0); /* Initial baud/probe discards old samples. */
    assert(writes == 1 && baud == 9600);
#endif
    receive(fix);
    gps_uart_poll(&model, 10);
    assert(model.speed_mph.valid && !model.demo && !model.fuel_percent.valid);
    assert(model.course_deg.valid && speed_model_view(&model, 10).course_valid);
    receive(fix); /* Already queued before the application stalls. */
    gps_uart_poll(&model, 3000);
    assert(flushes == 1 + 2*CONFIG_SPEED_GPS_AUTOCONFIG && !model.speed_mph.valid && buffered_size == 0);
    assert(!model.course_deg.valid);
    receive(fix);
    gps_uart_poll(&model, 3010);
#if CONFIG_SPEED_GPS_AUTOCONFIG
    assert(baud == 4800); /* Stalled probe has already resumed. */
#endif
    assert(model.speed_mph.valid);
    receive(fix);
    error_event = UART_FIFO_OVF;
    gps_uart_poll(&model, 3020);
    assert(!model.speed_mph.valid);
    assert(!model.course_deg.valid);
    receive(fix);
    gps_uart_poll(&model, 3030);
    assert(model.speed_mph.valid);
    memset(buffered, 'x', sizeof buffered);
    buffered_size = sizeof buffered;
    unsigned before = read_calls;
    gps_uart_poll(&model, 3040);
    assert(read_calls == before + 1 && buffered_size == sizeof buffered - 256);
    if (!CONFIG_SPEED_GPS_AUTOCONFIG) assert(writes == 0 && baud == 9600);
#if CONFIG_SPEED_GPS_AUTOCONFIG
    buffered_size=0; emulate=true; module_baud=38400; writes=0;
    assert(gps_uart_init() == ESP_OK);
    for (uint64_t t=0;t<=10000;t+=10) gps_uart_poll(&model,t);
    assert(module_baud == 9600 && baud == 9600 && writes == 10);
    receive(fix); gps_uart_poll(&model,10010); assert(model.speed_mph.valid);
    speed_model_set_speed(&model,NAN,10020);
    queue_frame(1,7,(const uint8_t *)fix,strlen(fix));
    gps_uart_poll(&model,10020); assert(!model.speed_mph.valid);
    receive(fix); gps_uart_poll(&model,10030); assert(model.speed_mph.valid);

    /* Driver failures must stop bounded setup and permit receive-only recovery. */
    emulate=false; fail_baud=true; writes=0; buffered_size=0;
    assert(gps_uart_init() == ESP_OK);
    for (uint64_t t=0;t<=1000;t+=10) gps_uart_poll(&model,t);
    assert(writes == 0);
    fail_baud=false; short_tx=true; writes=0;
    assert(gps_uart_init() == ESP_OK);
    for (uint64_t t=0;t<=1000;t+=10) gps_uart_poll(&model,t);
    assert(writes == 1 && baud == 9600);
    short_tx=false; receive(fix); gps_uart_poll(&model,1010); assert(model.speed_mph.valid);

    /* Wrong-baud traffic generates framing errors; it must not stall discovery. */
    writes=0; buffered_size=0; assert(gps_uart_init() == ESP_OK);
    gps_uart_poll(&model,0);
    for (uint64_t t=10;t<=10000;t+=10) {
        error_event=UART_FRAME_ERR; gps_uart_poll(&model,t);
    }
    assert(writes == 7 && baud == 9600);
#endif
    puts("PASS: UART pin/baud, bounded service, stalled input discard, overflow and recovery");
}
