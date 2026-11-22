#include "speed_model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void test_course(void)
{
    speed_model_t m = {0};
    assert(!speed_model_view(&m, 0).course_valid);
    speed_model_set_speed(&m, 20, 100);
    const float courses[] = {0, 45, 180, 359.4f, 359.6f};
    const int rounded[] = {0, 45, 180, 359, 0};
    for (unsigned i = 0; i < sizeof courses / sizeof *courses; ++i) {
        speed_model_set_course(&m, courses[i], 100);
        speed_view_t v = speed_model_view(&m, 100);
        assert(v.course_valid && v.course_deg == rounded[i]);
    }
    const float invalid[] = {-1, NAN, INFINITY, 360, 361};
    for (unsigned i = 0; i < sizeof invalid / sizeof *invalid; ++i) {
        speed_model_set_course(&m, invalid[i], 100);
        assert(!speed_model_view(&m, 100).course_valid);
        assert(speed_model_view(&m, 100).speed_valid);
    }
    speed_model_set_speed(&m, 1.99f, 100);
    speed_model_set_course(&m, 90, 100);
    assert(speed_model_view(&m, 100).mph == 2);
    assert(!speed_model_view(&m, 100).course_valid);
    speed_model_set_speed(&m, SPEED_COURSE_MIN_MPH, 100);
    speed_model_set_course(&m, 90, 100);
    assert(speed_model_view(&m, 100).course_valid);
    speed_model_set_speed(&m, NAN, 101);
    assert(!m.course_deg.valid);
    speed_model_set_speed(&m, 20, 102);
    assert(!speed_model_view(&m, 102).course_valid);
    speed_model_set_course(&m, 90, 100);
    speed_model_set_speed(&m, 20, 1000);
    assert(!speed_model_view(&m, 99).course_valid);
    assert(speed_model_view(&m, 2100).course_valid);
    assert(!speed_model_view(&m, 2101).course_valid);
    assert(speed_model_view(&m, 2101).speed_valid);
    speed_model_set_course(&m, 180, 3001);
    assert(!speed_model_view(&m, 3001).course_valid); /* Speed expired. */
    speed_model_set_speed(&m, 0, 3002);
    assert(!m.course_deg.valid && !speed_model_view(&m, 3002).course_valid);
}

int main(void)
{
    test_course();
    speed_model_t m = {0};
    speed_view_t v = speed_model_view(&m, 0);
    assert(!v.speed_valid && !v.fuel_valid && !v.demo);
    speed_model_set_speed(&m, 0, 10);
    speed_model_set_fuel(&m, 0, 10);
    v = speed_model_view(&m, 10);
    assert(v.speed_valid && v.mph == 0 && v.arc_mph == 0);
    assert(v.fuel_valid && v.fuel_percent == 0 && v.fuel_low);
    assert(speed_model_view(&m, 2010).speed_valid);
    assert(!speed_model_view(&m, 2011).speed_valid);
    speed_model_set_fuel(&m, 68.4f, 1010);
    v = speed_model_view(&m, 2011);
    assert(!v.speed_valid && v.fuel_valid && v.fuel_percent == 68);
    speed_model_set_speed(&m, 72.6f, 2011);
    v = speed_model_view(&m, 2011);
    assert(v.mph == 73 && v.arc_mph == SPEED_SCALE_MAX_MPH);
    assert(!speed_model_view(&m, 2010).speed_valid);
    const float bad_speed[] = {-1, NAN, INFINITY, 200.1f};
    const float bad_fuel[] = {-1, NAN, INFINITY, 100.1f};
    for (unsigned i = 0; i < 4; ++i) {
        speed_model_set_speed(&m, bad_speed[i], 5);
        speed_model_set_fuel(&m, bad_fuel[i], 5);
        v = speed_model_view(&m, 5);
        assert(!v.speed_valid && !v.fuel_valid && !v.fuel_low);
    }
    speed_model_set_fuel(&m, 14.9f, 5);
    assert(speed_model_view(&m, 5).fuel_low);
    speed_model_set_fuel(&m, 15, 5);
    assert(!speed_model_view(&m, 5).fuel_low);
    speed_model_set_fuel(&m, 100, 5);
    assert(speed_model_view(&m, 5).fuel_percent == 100);
    const uint64_t before_wrap = UINT64_C(0xffffffff) - 100;
    speed_model_set_speed(&m, 32.4f, before_wrap);
    assert(speed_model_view(&m, before_wrap + 2000).speed_valid);
    assert(!speed_model_view(&m, before_wrap + 2001).speed_valid);
    assert(!speed_model_view(&m, before_wrap + UINT64_C(0x100000000)).speed_valid);
    bool saw_low = false, saw_missing = false, saw_stopped = false, saw_course = false;
    for (uint64_t t = 0; t < 80000; t += 40) {
        speed_demo_sample(&m, t, t);
        v = speed_model_view(&m, t);
        assert(v.demo);
        saw_low |= v.fuel_low;
        saw_missing |= !v.speed_valid && !v.fuel_valid;
        saw_stopped |= v.speed_valid && v.mph == 0;
        saw_course |= v.course_valid;
        if (v.course_valid) assert(v.speed_valid && v.course_deg >= 0 && v.course_deg < 360);
        if (!v.speed_valid || v.mph == 0) assert(!v.course_valid);
        if (v.speed_valid) assert(v.mph >= 0 && v.mph <= 60);
    }
    assert(saw_low && saw_missing && saw_stopped && saw_course);
    puts("PASS: model validity, zero, bounds, independent expiry, long uptime and demo");
}
