#include "board.h"
#include "gps_uart.h"
#include "speed_model.h"
#include "speed_ui.h"
#include "speed_startup.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "speed";
static speed_ui_t ui;
static speed_model_t model;
static speed_startup_t startup;

static uint64_t milliseconds(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

static void toggle_night(lv_event_t *event)
{
    (void)event;
    speed_ui_set_night(&ui, !ui.night);
    ESP_ERROR_CHECK(board_set_brightness(ui.night ? CONFIG_SPEED_NIGHT_BRIGHTNESS :
                                                   CONFIG_SPEED_DAY_BRIGHTNESS));
}

void app_main(void)
{
#if CONFIG_SPEED_DEMO_MODE
    model.demo = true;
#endif
    ESP_LOGI(TAG, "Boat speed v0.1.0; reset reason=%d; source=%s",
             (int)esp_reset_reason(), model.demo ? "DEMO" : "GPS");
    ESP_LOGW(TAG, "Prototype: fuel sender and boat power interface not configured");
    ESP_ERROR_CHECK(board_init());
#if CONFIG_SPEED_GPS_MODE
    ESP_ERROR_CHECK(gps_uart_init());
#endif
    speed_ui_create(&ui);
    const speed_view_t initial = speed_model_view(&model, milliseconds());
    speed_ui_update(&ui, &initial);
    lv_obj_add_flag(ui.screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.screen, toggle_night, LV_EVENT_CLICKED, NULL);
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    speed_startup_create(&startup, ui.screen);
    lv_refr_now(NULL);
    ESP_ERROR_CHECK(board_set_brightness(CONFIG_SPEED_DAY_BRIGHTNESS));
    const uint64_t started = milliseconds();
    speed_startup_start(&startup, started);
    uint64_t last_tick = started, last_sample = started, gauge_started = started;
    bool first_sample = true, gauge_visible = false;
    for (;;) {
        const uint64_t now = milliseconds();
        lv_tick_inc((uint32_t)(now - last_tick));
        last_tick = now;
#if CONFIG_SPEED_GPS_MODE
        /* Drain during startup too: never show a ten-second-old queued fix. */
        gps_uart_poll(&model, now);
#endif
        if (!gauge_visible && speed_startup_update(&startup, now)) {
            gauge_visible = true;
            gauge_started = now;
        }
        if (gauge_visible && (first_sample || now - last_sample >= 40u)) {
#if CONFIG_SPEED_DEMO_MODE
            speed_demo_sample(&model, now - gauge_started, now);
#else
            (void)gauge_started;
#endif
            /* Future calibrated fuel provider publishes percent here on this task.
             * Never inject demo fuel into the live GPS model. */
            const speed_view_t view = speed_model_view(&model, now);
            speed_ui_update(&ui, &view);
            last_sample = now;
            first_sample = false;
        }
        lv_timer_handler();
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
