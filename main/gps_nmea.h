#ifndef GPS_NMEA_H
#define GPS_NMEA_H

#include "speed_model.h"
#include <stddef.h>

#define GPS_NMEA_LINE_SIZE 160

typedef struct {
    char line[GPS_NMEA_LINE_SIZE];
    size_t length;
    bool collecting;
} gps_nmea_t;

/* Initialize parser to {0}. Feed on the app task; never from an ISR. */
void gps_nmea_feed(gps_nmea_t *parser, char byte, speed_model_t *model, uint64_t now_ms);

#endif
