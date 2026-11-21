#include "speed_model.h"
#include <math.h>

void speed_model_set_speed(speed_model_t *model, float mph, uint64_t now_ms)
{
    model->speed_mph = (speed_reading_t){mph, now_ms,
        isfinite(mph) && mph >= 0 && mph <= SPEED_INPUT_MAX_MPH};
}

void speed_model_set_fuel(speed_model_t *model, float percent, uint64_t now_ms)
{
    model->fuel_percent = (speed_reading_t){percent, now_ms,
        isfinite(percent) && percent >= 0 && percent <= 100};
}

static bool fresh(speed_reading_t reading, uint64_t now_ms)
{
    return reading.valid && now_ms >= reading.updated_ms &&
           now_ms - reading.updated_ms <= SPEED_STALE_MS;
}

speed_view_t speed_model_view(const speed_model_t *model, uint64_t now_ms)
{
    speed_view_t view = {
        .speed_valid = fresh(model->speed_mph, now_ms),
        .fuel_valid = fresh(model->fuel_percent, now_ms),
        .demo = model->demo
    };
    if (view.speed_valid) {
        view.mph = (int)(model->speed_mph.value + 0.5f);
        view.arc_mph = view.mph > SPEED_SCALE_MAX_MPH ? SPEED_SCALE_MAX_MPH : view.mph;
    }
    if (view.fuel_valid) {
        view.fuel_percent = (int)(model->fuel_percent.value + 0.5f);
        view.fuel_low = model->fuel_percent.value < SPEED_FUEL_LOW_PERCENT;
    }
    return view;
}

void speed_demo_sample(speed_model_t *model, uint64_t elapsed_ms, uint64_t now_ms)
{
    const uint32_t t = (uint32_t)(elapsed_ms % 40000u);
    float mph = 0, fuel = 68;
    model->demo = true;
    if (t >= 38000u) {
        speed_model_set_speed(model, NAN, now_ms);
        speed_model_set_fuel(model, NAN, now_ms);
        return;
    }
    if (t >= 3000u && t < 9000u) mph = 4;
    else if (t >= 9000u && t < 18000u) mph = 4 + 32.0f * (t - 9000u) / 9000.0f;
    else if (t >= 18000u && t < 26000u) mph = 36 + 0.4f * sinf((float)t / 1100.0f);
    else if (t >= 26000u && t < 32000u) mph = 36 - 32.0f * (t - 26000u) / 6000.0f;
    else if (t >= 32000u) { mph = 4; fuel = t < 35000u ? 12 : 0; }
    speed_model_set_speed(model, mph, now_ms);
    speed_model_set_fuel(model, fuel, now_ms);
}
