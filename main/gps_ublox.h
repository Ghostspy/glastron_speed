#ifndef GPS_UBLOX_H
#define GPS_UBLOX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    GPS_UBLOX_PROBE, GPS_UBLOX_PORT, GPS_UBLOX_SWITCH,
    GPS_UBLOX_CHECK_PORT, GPS_UBLOX_RATE, GPS_UBLOX_RMC,
    GPS_UBLOX_CHECK_RATE, GPS_UBLOX_CHECK_RMC,
    GPS_UBLOX_READY, GPS_UBLOX_FALLBACK
} gps_ublox_phase_t;

typedef struct {
    unsigned baud; /* Zero means no baud change. Flush RX on a change. */
    uint8_t bytes[28];
    size_t size;
} gps_ublox_action_t;

typedef struct {
    gps_ublox_phase_t phase;
    unsigned target_baud, current_baud, probe_index, attempts;
    bool waiting, detected;
    uint64_t deadline_ms, frame_started_ms;
    char software[31], hardware[11];
    uint8_t rx_state, cls, id, a, b, checksum_a;
    uint16_t length, position;
    uint8_t payload[256];
} gps_ublox_t;

void gps_ublox_init(gps_ublox_t *gps, unsigned target_baud);
/* True means this byte belongs to binary traffic, not NMEA. */
bool gps_ublox_feed(gps_ublox_t *gps, uint8_t byte, uint64_t now_ms);
/* At most one action per call, no waiting or allocation. */
bool gps_ublox_next(gps_ublox_t *gps, uint64_t now_ms, gps_ublox_action_t *action);
void gps_ublox_rx_reset(gps_ublox_t *gps);

#endif
