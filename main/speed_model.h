#ifndef SPEED_MODEL_H
#define SPEED_MODEL_H

#include <stdbool.h>
#include <stdint.h>

#define SPEED_SCALE_MAX_MPH 60
#define SPEED_STALE_MS UINT64_C(2000)
#define SPEED_INPUT_MAX_MPH 200.0f
#define SPEED_FUEL_LOW_PERCENT 15.0f
/* Display policy: suppress GPS course below 2 MPH to limit low-speed wander. */
#define SPEED_COURSE_MIN_MPH 2.0f

typedef struct {
    float value;
    uint64_t updated_ms;
    bool valid;
} speed_reading_t;

typedef struct {
    speed_reading_t speed_mph;
    speed_reading_t course_deg;
    speed_reading_t fuel_percent;
    bool demo;
} speed_model_t;

typedef struct {
    bool speed_valid;
    bool course_valid;
    bool fuel_valid;
    bool fuel_low;
    bool demo;
    int mph;
    int arc_mph;
    int course_deg;
    int fuel_percent;
} speed_view_t;

void speed_model_set_speed(speed_model_t *model, float mph, uint64_t now_ms);
void speed_model_set_course(speed_model_t *model, float degrees_true, uint64_t now_ms);
void speed_model_set_fuel(speed_model_t *model, float percent, uint64_t now_ms);
speed_view_t speed_model_view(const speed_model_t *model, uint64_t now_ms);
void speed_demo_sample(speed_model_t *model, uint64_t elapsed_ms, uint64_t now_ms);

#endif
