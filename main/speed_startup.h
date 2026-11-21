#ifndef SPEED_STARTUP_H
#define SPEED_STARTUP_H

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

#define SPEED_STARTUP_DURATION_MS UINT64_C(10000)

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *progress;
    lv_obj_t *next_screen;
    uint64_t visible_since_ms;
    bool started;
    bool complete;
} speed_startup_t;

void speed_startup_create(speed_startup_t *startup, lv_obj_t *next_screen);
/* Call immediately after the first frame is visible, not during hardware init. */
void speed_startup_start(speed_startup_t *startup, uint64_t now_ms);
/* Returns true after loading next_screen; repeated calls are safe. */
bool speed_startup_update(speed_startup_t *startup, uint64_t now_ms);

#endif
