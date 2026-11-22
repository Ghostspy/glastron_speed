#include "speed_ui.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define FACE 0x000000
#define WHITE 0xF5F6F2
#define MUTED 0x819099
#define BLUE 0x65C7F0
#define AMBER 0xFFC15A
#define TRACK 0x26333D

LV_FONT_DECLARE(speed_font);

/* All point arrays outlive LVGL line objects, which retain their pointers. */
static lv_point_t ticks[31][2];
static const char *const scale_numbers[] = {"0", "10", "20", "30", "40", "50", "60"};
static const lv_point_t divider[] = {{170, 311}, {310, 311}};

static void set_text(lv_obj_t *obj, const char *text)
{
    if (strcmp(lv_label_get_text(obj), text)) lv_label_set_text_static(obj, text);
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                       uint32_t color, int x, int y, int width)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(obj, width);
    lv_obj_set_pos(obj, x, y);
    lv_label_set_text_static(obj, text);
    return obj;
}

static void line(lv_obj_t *parent, const lv_point_t *points, int count,
                 int width, uint32_t color)
{
    lv_obj_t *obj = lv_line_create(parent);
    lv_line_set_points(obj, points, (uint16_t)count);
    lv_obj_set_style_line_width(obj, width, 0);
    lv_obj_set_style_line_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_line_rounded(obj, true, 0);
}

void speed_ui_create(speed_ui_t *ui)
{
    memset(ui, 0, sizeof(*ui));
    ui->screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(ui->screen);
    lv_obj_set_size(ui->screen, 480, 480);
    lv_obj_set_style_bg_color(ui->screen, lv_color_hex(FACE), 0);
    lv_obj_set_style_bg_opa(ui->screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(ui->screen, LV_OBJ_FLAG_SCROLLABLE);

    ui->arc = lv_arc_create(ui->screen);
    lv_obj_remove_style_all(ui->arc);
    lv_obj_set_size(ui->arc, 454, 454);
    lv_obj_set_pos(ui->arc, 13, 13);
    lv_arc_set_rotation(ui->arc, 135);
    lv_arc_set_bg_angles(ui->arc, 0, 270);
    lv_arc_set_range(ui->arc, 0, SPEED_SCALE_MAX_MPH);
    lv_obj_set_style_arc_width(ui->arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui->arc, lv_color_hex(TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(ui->arc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui->arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui->arc, lv_color_hex(BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(ui->arc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(ui->arc, true, LV_PART_INDICATOR);
    lv_obj_clear_flag(ui->arc, LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i <= 30; ++i) {
        const float a = (135.0f + 270.0f * (float)i / 30.0f) * 0.01745329252f;
        const bool major = i % 5 == 0;
        const float r = major ? 194.0f : 204.0f;
        ticks[i][0] = (lv_point_t){(lv_coord_t)lroundf(240 + r * cosf(a)),
                                  (lv_coord_t)lroundf(240 + r * sinf(a))};
        ticks[i][1] = (lv_point_t){(lv_coord_t)lroundf(240 + 214 * cosf(a)),
                                  (lv_coord_t)lroundf(240 + 214 * sinf(a))};
        line(ui->screen, ticks[i], 2, major ? 3 : 1, major ? WHITE : MUTED);
        if (major) {
            const int x = (int)lroundf(240 + 173 * cosf(a)) - 22;
            const int y = (int)lroundf(240 + 173 * sinf(a)) - 14;
            label(ui->screen, scale_numbers[i / 5], &lv_font_montserrat_24,
                  WHITE, x, y, 44);
        }
    }

    label(ui->screen, "GLASTRON", &lv_font_montserrat_18, WHITE, 130, 104, 220);
    label(ui->screen, "SSV-175", &lv_font_montserrat_14, MUTED, 170, 133, 140);
    ui->course = label(ui->screen, "--", &lv_font_montserrat_18, MUTED, 130, 162, 220);
    ui->speed = label(ui->screen, "--", &speed_font, WHITE, 120, 187, 240);
    label(ui->screen, "MPH", &lv_font_montserrat_18, MUTED, 190, 278, 100);
    line(ui->screen, divider, 2, 1, TRACK);
    ui->fuel = label(ui->screen, "-- %", &lv_font_montserrat_32,
                     WHITE, 140, 330, 200);
    ui->fuel_caption = label(ui->screen, "Fuel unavailable", &lv_font_montserrat_16,
                             MUTED, 125, 371, 230);
    ui->fuel_bar = lv_bar_create(ui->screen);
    lv_obj_remove_style_all(ui->fuel_bar);
    lv_obj_set_size(ui->fuel_bar, 160, 5);
    lv_obj_set_pos(ui->fuel_bar, 160, 399);
    lv_bar_set_range(ui->fuel_bar, 0, 100);
    lv_obj_set_style_bg_color(ui->fuel_bar, lv_color_hex(TRACK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->fuel_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui->fuel_bar, lv_color_hex(BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui->fuel_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_clear_flag(ui->fuel_bar, LV_OBJ_FLAG_CLICKABLE);
    ui->mode = label(ui->screen, "GPS UNAVAILABLE", &lv_font_montserrat_14, AMBER, 110, 420, 260);
    ui->theme_hint = label(ui->screen, "", &lv_font_montserrat_14, MUTED, 175, 444, 130);
    lv_scr_load(ui->screen);
}

void speed_ui_set_night(speed_ui_t *ui, bool night)
{
    ui->night = night;
    lv_obj_set_style_arc_color(ui->arc, lv_color_hex(night ? AMBER : BLUE), LV_PART_INDICATOR);
    lv_label_set_text_static(ui->theme_hint, night ? "Night" : "");
}

void speed_ui_update(speed_ui_t *ui, const speed_view_t *view)
{
    static const char *const compass[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    char text[32];
    if (view->course_valid)
        snprintf(text, sizeof text, "%s • %03d°T", compass[((view->course_deg + 22) / 45) % 8], view->course_deg);
    else snprintf(text, sizeof text, "--");
    if (strcmp(lv_label_get_text(ui->course), text)) {
        strcpy(ui->course_text, text);
        lv_label_set_text_static(ui->course, ui->course_text);
    }
    lv_obj_set_style_text_color(ui->course,
        lv_color_hex(view->course_valid ? ui->night ? AMBER : BLUE : MUTED), 0);
    if (view->speed_valid) snprintf(text, sizeof text, "%d", view->mph);
    else snprintf(text, sizeof text, "--");
    if (strcmp(lv_label_get_text(ui->speed), text)) {
        strcpy(ui->speed_text, text);
        lv_label_set_text_static(ui->speed, ui->speed_text);
    }
    if (view->fuel_valid) snprintf(text, sizeof text, "%d %%", view->fuel_percent);
    else snprintf(text, sizeof text, "-- %%");
    if (strcmp(lv_label_get_text(ui->fuel), text)) {
        strcpy(ui->fuel_text, text);
        lv_label_set_text_static(ui->fuel, ui->fuel_text);
    }
    lv_arc_set_value(ui->arc, view->speed_valid ? view->arc_mph : 0);
    lv_bar_set_value(ui->fuel_bar, view->fuel_valid ? view->fuel_percent : 0, LV_ANIM_OFF);
    lv_obj_set_style_text_color(ui->speed, lv_color_hex(view->speed_valid ? WHITE : MUTED), 0);
    lv_obj_set_style_text_color(ui->fuel, lv_color_hex(view->fuel_low ? AMBER :
                                view->fuel_valid ? WHITE : MUTED), 0);
    lv_obj_set_style_text_color(ui->fuel_caption, lv_color_hex(view->fuel_low ? AMBER : MUTED), 0);
    lv_obj_set_style_bg_color(ui->fuel_bar,
        lv_color_hex(view->fuel_low || ui->night ? AMBER : BLUE), LV_PART_INDICATOR);
    set_text(ui->fuel_caption, view->fuel_low ? "Low fuel" :
        view->fuel_valid ? "Fuel" : "Fuel unavailable");
    set_text(ui->mode, view->demo ? "DEMO" : view->speed_valid ? "GPS" : "GPS UNAVAILABLE");
}
