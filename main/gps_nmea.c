#include "gps_nmea.h"
#include <math.h>
#include <string.h>

#define MPH_PER_KNOT 1.150779448f

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool decimal_number(const char *s, float *value)
{
    if (*s < '0' || *s > '9') return false;
    float number = 0, fraction = 0.1f;
    bool point = false;
    unsigned digits_after_point = 0;
    for (; *s; ++s) {
        if (*s == '.' && !point) { point = true; continue; }
        if (*s < '0' || *s > '9') return false;
        if (point) { number += (*s - '0') * fraction; fraction *= 0.1f; ++digits_after_point; }
        else number = number * 10 + (*s - '0');
        if (!isfinite(number)) return false;
    }
    if (point && !digits_after_point) return false;
    *value = number;
    return true;
}

static void parse(char *line, speed_model_t *model, uint64_t now_ms)
{
    char *star = strchr(line, '*');
    if (!star || strlen(star) != 3) return;
    const int hi = hex_digit(star[1]), lo = hex_digit(star[2]);
    if (hi < 0 || lo < 0) return;
    unsigned checksum = 0;
    for (const char *s = line; s < star; ++s) checksum ^= (unsigned char)*s;
    if (checksum != (unsigned)((hi << 4) | lo)) return;
    *star = '\0';
    /* Do not let unrelated traffic modify freshness or fix validity. */
    if (strlen(line) < 6 || line[0] < 'A' || line[0] > 'Z' ||
        line[1] < 'A' || line[1] > 'Z' || strncmp(line + 2, "RMC,", 4)) return;
    char *fields[16] = {line};
    size_t count = 1;
    bool overflow = false;
    for (char *s = line; *s; ++s) {
        if (*s == ',') {
            *s = '\0';
            if (count == sizeof fields / sizeof *fields) { overflow = true; break; }
            fields[count++] = s + 1;
        }
    }
    float knots = NAN, mph = NAN, course = NAN;
    const bool measured_mode = count == 12 || (count >= 13 && strlen(fields[12]) == 1 &&
                                               strchr("ADPRF", fields[12][0]) != NULL);
    const bool valid_fix = !overflow && count >= 12 && count <= 14 &&
        !strcmp(fields[2], "A") && measured_mode && decimal_number(fields[7], &knots) &&
        knots * MPH_PER_KNOT <= SPEED_INPUT_MAX_MPH;
    if (valid_fix) {
        mph = knots * MPH_PER_KNOT;
        /* A missing/malformed course must not discard an otherwise valid speed. */
        if (!decimal_number(fields[8], &course) || course >= 360) course = NAN;
    }
    speed_model_set_speed(model, mph, now_ms);
    speed_model_set_course(model, course, now_ms);
}

void gps_nmea_feed(gps_nmea_t *parser, char byte, speed_model_t *model, uint64_t now_ms)
{
    if (byte == '$') {
        parser->collecting = true;
        parser->length = 0;
        parser->started_ms = now_ms;
        return;
    }
    if (!parser->collecting) return;
    if (now_ms < parser->started_ms || now_ms - parser->started_ms > SPEED_STALE_MS) {
        parser->collecting = false;
        parser->length = 0;
        return;
    }
    if (byte == '\n') {
        if (parser->length && parser->line[parser->length - 1] == '\r') --parser->length;
        parser->line[parser->length] = '\0';
        parse(parser->line, model, now_ms);
        parser->collecting = false;
        parser->length = 0;
        return;
    }
    if ((parser->length && parser->line[parser->length - 1] == '\r') ||
        parser->length >= sizeof parser->line - 1 ||
        ((unsigned char)byte < 32 && byte != '\r') || (unsigned char)byte > 126) {
        parser->collecting = false;
        parser->length = 0;
        return;
    }
    parser->line[parser->length++] = byte;
}
