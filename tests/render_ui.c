#include "speed_ui.h"
#include "speed_startup.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static lv_color_t framebuffer[480 * 480];
static lv_color_t draw_buffer[480 * 40];

static void flush(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels)
{
    for (int y = area->y1; y <= area->y2; ++y)
        for (int x = area->x1; x <= area->x2; ++x) {
            if (x >= 0 && x < 480 && y >= 0 && y < 480) framebuffer[y * 480 + x] = *pixels;
            ++pixels;
        }
    lv_disp_flush_ready(driver);
}

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "Usage: render-ui SCENE output.ppm\n"); return 2; }
    lv_init();
    static lv_disp_draw_buf_t buffer;
    static lv_disp_drv_t driver;
    lv_disp_draw_buf_init(&buffer, draw_buffer, NULL, 480 * 40);
    lv_disp_drv_init(&driver);
    driver.hor_res = driver.ver_res = 480;
    driver.draw_buf = &buffer;
    driver.flush_cb = flush;
    assert(lv_disp_drv_register(&driver));
    speed_ui_t ui;
    speed_ui_create(&ui);
    speed_startup_t startup;
    const bool startup_scene = !strcmp(argv[1], "startup");
    if (startup_scene) {
        speed_startup_create(&startup, ui.screen);
        speed_startup_start(&startup, 0);
        assert(!speed_startup_update(&startup, 5000));
    }
    speed_model_t model = {.demo = true};
    speed_model_set_speed(&model, 32, 0);
    speed_model_set_fuel(&model, 68, 0);
    if (!strcmp(argv[1], "low")) speed_model_set_fuel(&model, 12, 0);
    else if (!strcmp(argv[1], "empty")) speed_model_set_fuel(&model, 0, 0);
    else if (!strcmp(argv[1], "missing") || !strcmp(argv[1], "gps-waiting")) model = (speed_model_t){0};
    else if (!strcmp(argv[1], "speed-missing")) { model.speed_mph.valid = false; model.demo = false; }
    else if (!strcmp(argv[1], "fuel-missing") || !strcmp(argv[1], "gps")) { model.fuel_percent.valid = false; model.demo = false; }
    else if (!strcmp(argv[1], "stopped")) speed_model_set_speed(&model, 0, 0);
    else if (!strcmp(argv[1], "night")) speed_ui_set_night(&ui, true);
    else if (strcmp(argv[1], "normal") && !startup_scene) { fprintf(stderr, "Unknown scene\n"); return 2; }
    speed_view_t view = speed_model_view(&model, 0);
    speed_ui_update(&ui, &view);
    assert(!strcmp(lv_label_get_text(ui.speed), view.speed_valid ? view.mph ? "32" : "0" : "--"));
    assert(!strcmp(lv_label_get_text(ui.mode), view.demo ? "DEMO" : view.speed_valid ? "GPS" : "GPS UNAVAILABLE"));
    for (int i = 0; i < 5; ++i) { lv_tick_inc(40); lv_timer_handler(); }
    unsigned visible_pixels = 0;
    for (int y = 190; y < 270; ++y)
        for (int x = 160; x < 320; ++x)
            visible_pixels += framebuffer[y * 480 + x].full != 0;
    assert(visible_pixels > 200);

    speed_model_set_speed(&model, 0, 500);
    speed_model_set_fuel(&model, 0, 500);
    view = speed_model_view(&model, 500);
    speed_ui_update(&ui, &view);
    speed_ui_update(&ui, &view);
    assert(!strcmp(lv_label_get_text(ui.speed), "0"));
    assert(!strcmp(lv_label_get_text(ui.fuel), "0 %"));
    assert(!strcmp(lv_label_get_text(ui.fuel_caption), "Low fuel"));
    view = speed_model_view(&model, 2501);
    speed_ui_update(&ui, &view);
    assert(!strcmp(lv_label_get_text(ui.speed), "--"));
    assert(!strcmp(lv_label_get_text(ui.fuel), "-- %"));
    assert(!strcmp(lv_label_get_text(ui.fuel_caption), "Fuel unavailable"));
    assert(lv_arc_get_value(ui.arc) == 0 && lv_bar_get_value(ui.fuel_bar) == 0);
    speed_model_set_speed(&model, 32, 2600);
    speed_model_set_fuel(&model, 68, 2600);
    view = speed_model_view(&model, 2600);
    speed_ui_update(&ui, &view);
    assert(!strcmp(lv_label_get_text(ui.speed), "32"));
    assert(lv_bar_get_value(ui.fuel_bar) == 68);
    assert(!strcmp(lv_label_get_text(ui.fuel_caption), "Fuel"));

    /* Preserve original scene: no LVGL refresh follows transition assertions. */
    FILE *out = fopen(argv[2], "wb");
    if (!out) { perror(argv[2]); return 1; }
    fprintf(out, "P6\n480 480\n255\n");
    for (int y = 0; y < 480; ++y)
        for (int x = 0; x < 480; ++x) {
            lv_color32_t c = {.full = lv_color_to32(framebuffer[y * 480 + x])};
            if ((x - 240) * (x - 240) + (y - 240) * (y - 240) > 240 * 240)
                c.ch.red = c.ch.green = c.ch.blue = 38;
            const unsigned char rgb[] = {c.ch.red, c.ch.green, c.ch.blue};
            if (fwrite(rgb, 1, 3, out) != 3) return 1;
        }
    return fclose(out) == 0 ? 0 : 1;
}
