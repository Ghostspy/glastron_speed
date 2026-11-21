#include "gps_uart.h"
#include "gps_nmea.h"
#include "gps_ublox.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "sdkconfig.h"
#include <math.h>
#include <string.h>

/* Board's switched UART header; USB TO UART must be disconnected for GPS. */
#define GPS_UART UART_NUM_1
#define GPS_RX_GPIO 44
#define GPS_TX_GPIO 43
#define GPS_RX_BUFFER 2048
#define GPS_EVENT_QUEUE 16
#define GPS_POLL_BYTES 256

static gps_nmea_t parser;
static QueueHandle_t events;
static uint64_t last_poll_ms;
static bool polled;
#if CONFIG_SPEED_GPS_AUTOCONFIG
/* Application/bootloader logging must not write into the GPS command stream. */
#if CONFIG_ESP_CONSOLE_UART
#error "GPS autoconfiguration requires a native USB console (GPIO43 is GPS TX)"
#endif
static gps_ublox_t receiver;
static gps_ublox_phase_t reported_phase;
static bool reported_detection;
#endif
static const char *TAG = "gps";

esp_err_t gps_uart_init(void)
{
    const uart_config_t config = {
        .baud_rate = CONFIG_SPEED_GPS_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };
    esp_err_t err = uart_param_config(GPS_UART, &config);
    if (err != ESP_OK) return err;
    err = uart_set_pin(GPS_UART,
#if CONFIG_SPEED_GPS_AUTOCONFIG
                       GPS_TX_GPIO,
#else
                       UART_PIN_NO_CHANGE,
#endif
                       GPS_RX_GPIO,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;
    memset(&parser, 0, sizeof parser);
    polled = false;
#if CONFIG_SPEED_GPS_AUTOCONFIG
    gps_ublox_init(&receiver, CONFIG_SPEED_GPS_BAUD);
    reported_phase = GPS_UBLOX_PROBE;
    reported_detection = false;
    ESP_LOGI(TAG, "u-blox detection enabled; GPS RX wire required on GPIO%d", GPS_TX_GPIO);
#endif
    err = uart_driver_install(GPS_UART, GPS_RX_BUFFER, 0, GPS_EVENT_QUEUE, &events, 0);
    if (err != ESP_OK) return err;
    ESP_LOGI(TAG, "NMEA RMC RX: UART1 GPIO%d, %d baud, 8N1", GPS_RX_GPIO, CONFIG_SPEED_GPS_BAUD);
    ESP_LOGW(TAG, "External UART is disabled while USB TO UART is plugged in");
    return ESP_OK;
}

void gps_uart_poll(speed_model_t *model, uint64_t now_ms)
{
    uart_event_t event;
    /* UART buffers carry no measurement timestamps. Discard them after a stall
     * instead of assigning old packets a fresh time when the task resumes. */
    bool damaged = polled && (now_ms < last_poll_ms || now_ms - last_poll_ms > SPEED_STALE_MS);
    polled = true;
    last_poll_ms = now_ms;
    for (unsigned i = 0; i < GPS_EVENT_QUEUE && xQueueReceive(events, &event, 0); ++i) {
        if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL ||
            event.type == UART_PARITY_ERR || event.type == UART_FRAME_ERR ||
            event.type == UART_BREAK) damaged = true;
    }
    if (damaged) {
        (void)uart_flush_input(GPS_UART);
        xQueueReset(events);
        memset(&parser, 0, sizeof parser);
#if CONFIG_SPEED_GPS_AUTOCONFIG
        gps_ublox_rx_reset(&receiver);
#endif
        speed_model_set_speed(model, NAN, now_ms);
#if CONFIG_SPEED_GPS_AUTOCONFIG
        if (receiver.phase == GPS_UBLOX_READY || receiver.phase == GPS_UBLOX_FALLBACK)
#endif
            ESP_LOGW(TAG, "UART stream lost; waiting for a fresh GPS fix");
        goto configuration;
    }
    uint8_t bytes[GPS_POLL_BYTES];
    const int count = uart_read_bytes(GPS_UART, bytes, sizeof bytes, 0);
    if (count < 0) {
        memset(&parser, 0, sizeof parser);
#if CONFIG_SPEED_GPS_AUTOCONFIG
        gps_ublox_rx_reset(&receiver);
#endif
        speed_model_set_speed(model, NAN, now_ms);
        goto configuration;
    }
    for (int i = 0; i < count; ++i) {
#if CONFIG_SPEED_GPS_AUTOCONFIG
        if (gps_ublox_feed(&receiver, bytes[i], now_ms)) {
            /* Never interpret binary payload as an embedded NMEA measurement. */
            memset(&parser, 0, sizeof parser);
            continue;
        }
#endif
        gps_nmea_feed(&parser, (char)bytes[i], model, now_ms);
    }
configuration: ; /* UART errors must not starve baud-scan timeouts. */
#if CONFIG_SPEED_GPS_AUTOCONFIG
    gps_ublox_action_t action;
    if (gps_ublox_next(&receiver, now_ms, &action)) {
        esp_err_t err = ESP_OK;
        if (action.baud) {
            err = uart_set_baudrate(GPS_UART, action.baud);
            (void)uart_flush_input(GPS_UART);
            xQueueReset(events);
            memset(&parser, 0, sizeof parser);
            gps_ublox_rx_reset(&receiver);
            speed_model_set_speed(model, NAN, now_ms);
        }
        /* TX buffer disabled: write <=28 bytes into the 128-byte hardware FIFO.
         * No FIFO drain wait; this task is the sole UART1 writer. */
        if (err != ESP_OK || (action.size &&
            uart_tx_chars(GPS_UART, (const char *)action.bytes, (uint32_t)action.size) != (int)action.size)) {
            const bool restoring = receiver.phase == GPS_UBLOX_FALLBACK;
            receiver.phase = GPS_UBLOX_FALLBACK;
            receiver.waiting = restoring; /* One restore attempt, even on driver failure. */
            speed_model_set_speed(model, NAN, now_ms);
            ESP_LOGW(TAG, "GPS configuration UART operation failed");
        }
    }
    if (receiver.detected && !reported_detection) {
        reported_detection = true;
        ESP_LOGI(TAG, "u-blox-compatible receiver: SW=%.30s HW=%.10s", receiver.software, receiver.hardware);
    }
    if (receiver.phase != reported_phase) {
        reported_phase = receiver.phase;
        if (receiver.phase == GPS_UBLOX_READY)
            ESP_LOGI(TAG, "GPS settings verified: %d baud, 8N1, RMC 1 Hz (RAM only)", CONFIG_SPEED_GPS_BAUD);
        else if (receiver.phase == GPS_UBLOX_FALLBACK)
            ESP_LOGW(TAG, "GPS setup unconfirmed; listening at %d baud. Check RX wire/settings; restart to retry",
                     CONFIG_SPEED_GPS_BAUD);
    }
#endif
}
