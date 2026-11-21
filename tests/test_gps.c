#include "gps_nmea.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void feed(gps_nmea_t *p, speed_model_t *m, const char *s, uint64_t now)
{
    while (*s) gps_nmea_feed(p, *s++, m, now);
}

static void sentence(gps_nmea_t *p, speed_model_t *m, const char *body, uint64_t now)
{
    unsigned checksum = 0;
    for (const char *s = body; *s; ++s) checksum ^= (unsigned char)*s;
    char line[256];
    snprintf(line, sizeof line, "$%s*%02X\r\n", body, checksum);
    feed(p, m, line, now);
}

int main(void)
{
    gps_nmea_t p = {0};
    speed_model_t m = {0};
    /* Published classic RMC example: 22.4 knots = 25.78 MPH. */
    feed(&p, &m, "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r", 100);
    assert(!m.speed_mph.valid);
    feed(&p, &m, "\n", 110);
    assert(fabsf(m.speed_mph.value - 25.77746f) < 0.001f);
    assert(m.speed_mph.updated_ms == 110 && !m.demo && !m.fuel_percent.valid);
    /* Split/concatenated sentences, GN talker, valid zero, modern mode. */
    sentence(&p, &m, "GNRMC,123520,A,4807.038,N,01131.000,E,0.0,084.4,230394,,,A", 200);
    assert(speed_model_view(&m, 200).speed_valid && m.speed_mph.value == 0);
    feed(&p, &m, "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*00\r\n", 300);
    assert(m.speed_mph.updated_ms == 200);
    sentence(&p, &m, "GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,", 400);
    assert(m.speed_mph.updated_ms == 200);
    assert(!speed_model_view(&m, 2201).speed_valid);
    sentence(&p, &m, "GNRMC,123520,V,,,,,,,230394,,,N", 2300);
    assert(!m.speed_mph.valid);
    const char *bad[] = {"", "-1", "nan", "inf", "1e1", "2junk", " 2", "+2", "1.2.3", ".", "999999999999999999999999999999999999999"};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; ++i) {
        sentence(&p, &m, "GPRMC,123520,A,4807.038,N,01131.000,E,10,0,230394,,,A", 2500);
        char body[200];
        snprintf(body, sizeof body, "GPRMC,123520,A,4807.038,N,01131.000,E,%s,0,230394,,,A", bad[i]);
        sentence(&p, &m, body, 2501);
        assert(!m.speed_mph.valid);
    }
    const char *modes[] = {"N", "E", "M", "S", "X", "AA"};
    for (unsigned i = 0; i < sizeof modes / sizeof *modes; ++i) {
        char body[160];
        speed_model_set_speed(&m, 20, 2600);
        snprintf(body, sizeof body, "GPRMC,123520,A,4807.038,N,01131.000,E,10,0,230394,,,%s", modes[i]);
        sentence(&p, &m, body, 2601);
        assert(!m.speed_mph.valid);
    }
    /* A damaged/truncated stream cannot publish; a new '$' resynchronizes. */
    feed(&p, &m, "noise$GPRMC,broken", 2700);
    sentence(&p, &m, "GNRMC,123520,A,4807.038,N,01131.000,E,10,0,230394,,,D", 2701);
    assert(m.speed_mph.valid);
    feed(&p, &m, "$", 2800);
    for (int i = 0; i < 1000; ++i) gps_nmea_feed(&p, 'x', &m, 2800);
    feed(&p, &m, "\n", 2800);
    assert(m.speed_mph.updated_ms == 2701);
    sentence(&p, &m, "GLRMC,123520,A,4807.038,N,01131.000,E,20,0,230394,,,A,V", 2900);
    assert(m.speed_mph.valid && m.speed_mph.updated_ms == 2900);
    /* A checksum-valid truncated RMC must not leave the old reading valid. */
    sentence(&p, &m, "GPRMC,123520,A", 3000);
    assert(!m.speed_mph.valid);
    /* Deterministic arbitrary input under ASan/UBSan, followed by recovery. */
    unsigned rng = 7;
    for (int i = 0; i < 100000; ++i) {
        rng = rng * 1664525u + 1013904223u;
        gps_nmea_feed(&p, (char)(rng >> 24), &m, 4000);
    }
    sentence(&p, &m, "GPRMC,123520,A,4807.038,N,01131.000,E,10,0,230394,,,A", UINT64_C(0x100000010));
    assert(m.speed_mph.valid && m.speed_mph.updated_ms == UINT64_C(0x100000010));
    puts("PASS: RMC conversion, zero, checksum, fix validity, modes, bounds and stream recovery");
}
