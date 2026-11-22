#ifndef SPEED_UI_H
#define SPEED_UI_H

#include "lvgl.h"
#include "speed_model.h"

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *arc;
    lv_obj_t *course;
    lv_obj_t *speed;
    lv_obj_t *fuel;
    lv_obj_t *fuel_caption;
    lv_obj_t *fuel_bar;
    lv_obj_t *mode;
    lv_obj_t *theme_hint;
    bool night;
    char speed_text[16];
    char course_text[32];
    char fuel_text[16];
} speed_ui_t;

void speed_ui_create(speed_ui_t *ui);
void speed_ui_update(speed_ui_t *ui, const speed_view_t *view);
void speed_ui_set_night(speed_ui_t *ui, bool night);

#endif
