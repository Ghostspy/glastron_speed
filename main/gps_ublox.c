#include "gps_ublox.h"
#include <string.h>

#define RESPONSE_MS 1200u
#define SWITCH_MS 150u
static const unsigned bauds[] = {4800, 9600, 19200, 38400, 57600, 115200, 230400};
static const uint8_t rate[] = {0xe8, 3, 1, 0, 1, 0}; /* 1000 ms, nav=1, GPS time. */

static void advance(gps_ublox_t *g, gps_ublox_phase_t phase)
{
    g->phase = phase;
    g->waiting = false;
    g->attempts = 0;
}

void gps_ublox_rx_reset(gps_ublox_t *g) { g->rx_state = 0; }

void gps_ublox_init(gps_ublox_t *g, unsigned target_baud)
{
    memset(g, 0, sizeof *g);
    g->target_baud = target_baud;
    g->current_baud = target_baud;
}

static unsigned frame_timeout(const gps_ublox_t *g)
{
    const unsigned wire_ms = ((sizeof g->payload + 8u) * 10000u + g->current_baud - 1) / g->current_baud;
    return wire_ms + 100u > 500u ? wire_ms + 100u : 500u;
}

static uint32_t le32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static void packet(gps_ublox_t *g)
{
    const uint8_t *p = g->payload;
    if (!g->waiting) return;
    if (g->phase == GPS_UBLOX_PROBE && g->cls == 0x0a && g->id == 4 &&
        g->length >= 40 && (g->length - 40) % 30 == 0 &&
        p[0] && p[30] && memchr(p, 0, 30) && memchr(p + 30, 0, 10)) {
        memcpy(g->software, p, 30);
        memcpy(g->hardware, p + 30, 10);
        g->detected = true;
        advance(g, GPS_UBLOX_PORT);
    } else if ((g->phase == GPS_UBLOX_RATE || g->phase == GPS_UBLOX_RMC) &&
               g->cls == 5 && g->length == 2 && p[0] == 6 &&
               p[1] == (g->phase == GPS_UBLOX_RATE ? 8 : 1)) {
        if (g->id == 0) advance(g, GPS_UBLOX_FALLBACK);
        else if (g->id == 1)
            advance(g, g->phase == GPS_UBLOX_RATE ? GPS_UBLOX_RMC : GPS_UBLOX_CHECK_RATE);
    } else if (g->phase == GPS_UBLOX_CHECK_PORT && g->cls == 6 && g->id == 0 &&
               g->length == 20 && p[0] == 1) {
        const bool good = le32(p + 4) == 0x8d0 && le32(p + 8) == g->target_baud &&
                          (p[12] & 3) == 3 && (p[14] & 3) == 3;
        advance(g, good ? GPS_UBLOX_RATE : GPS_UBLOX_FALLBACK);
    } else if (g->phase == GPS_UBLOX_CHECK_RATE && g->cls == 6 && g->id == 8 && g->length == 6) {
        advance(g, memcmp(p, rate, 6) == 0 ? GPS_UBLOX_CHECK_RMC : GPS_UBLOX_FALLBACK);
    } else if (g->phase == GPS_UBLOX_CHECK_RMC && g->cls == 6 && g->id == 1 &&
               (g->length == 3 || g->length == 8) && p[0] == 0xf0 && p[1] == 4) {
        advance(g, p[g->length == 3 ? 2 : 3] == 1 ? GPS_UBLOX_READY : GPS_UBLOX_FALLBACK);
    }
}

bool gps_ublox_feed(gps_ublox_t *g, uint8_t c, uint64_t now)
{
    /* Total-frame deadline also bounds a corrupted length on a busy NMEA stream. */
    if (g->rx_state && (now < g->frame_started_ms ||
        now - g->frame_started_ms > frame_timeout(g)))
        gps_ublox_rx_reset(g);
    if (!g->rx_state) {
        if (c != 0xb5) return false;
        g->rx_state = 1;
        g->frame_started_ms = now;
        return true;
    }
    if (g->rx_state == 1) {
        g->rx_state = c == 0x62 ? 2 : (c == 0xb5 ? 1 : 0);
        g->a = g->b = 0;
        return c == 0x62 || c == 0xb5;
    }
    if (g->rx_state < 7) { g->a += c; g->b += g->a; }
    switch (g->rx_state) {
    case 2: g->cls = c; g->rx_state = 3; break;
    case 3: g->id = c; g->rx_state = 4; break;
    case 4: g->length = c; g->rx_state = 5; break;
    case 5:
        g->length |= (uint16_t)c << 8;
        g->position = 0;
        g->rx_state = g->length ? 6 : 7;
        break;
    case 6:
        if (g->position < sizeof g->payload) g->payload[g->position] = c;
        if (++g->position == g->length) g->rx_state = 7;
        break;
    case 7: g->checksum_a = c; g->rx_state = 8; break;
    case 8:
        if (g->checksum_a == g->a && c == g->b && g->length <= sizeof g->payload) packet(g);
        gps_ublox_rx_reset(g);
        break;
    default: gps_ublox_rx_reset(g); break;
    }
    return true;
}

static void encode(gps_ublox_action_t *out, uint8_t cls, uint8_t id, const uint8_t *p, size_t n)
{
    out->bytes[0] = 0xb5; out->bytes[1] = 0x62;
    out->bytes[2] = cls; out->bytes[3] = id;
    out->bytes[4] = (uint8_t)n; out->bytes[5] = 0;
    if (n) memcpy(out->bytes + 6, p, n);
    uint8_t a = 0, b = 0;
    for (size_t i = 2; i < n + 6; ++i) { a += out->bytes[i]; b += a; }
    out->bytes[n + 6] = a; out->bytes[n + 7] = b;
    out->size = n + 8;
}

bool gps_ublox_next(gps_ublox_t *g, uint64_t now, gps_ublox_action_t *out)
{
    memset(out, 0, sizeof *out);
    if (g->phase == GPS_UBLOX_READY) return false;
    if (g->phase == GPS_UBLOX_FALLBACK) {
        if (g->waiting) return false;
        g->waiting = true;
        out->baud = g->target_baud;
        g->current_baud = out->baud;
        return true;
    }
    if (g->waiting) {
        if (now < g->deadline_ms) return false;
        if (g->phase == GPS_UBLOX_SWITCH) advance(g, GPS_UBLOX_CHECK_PORT);
        else if (g->phase == GPS_UBLOX_PROBE) {
            do { ++g->probe_index; }
            while (g->probe_index <= sizeof bauds / sizeof *bauds &&
                   bauds[g->probe_index - 1] == g->target_baud);
            if (g->probe_index > sizeof bauds / sizeof *bauds) {
                advance(g, GPS_UBLOX_FALLBACK);
                return gps_ublox_next(g, now, out);
            }
        } else if (g->attempts >= 2) {
            advance(g, GPS_UBLOX_FALLBACK);
            return gps_ublox_next(g, now, out);
        }
    }
    const uint8_t port_id = 1, rmc[] = {0xf0, 4, 1};
    switch (g->phase) {
    case GPS_UBLOX_PROBE:
        out->baud = g->probe_index ? bauds[g->probe_index - 1] : g->target_baud;
        encode(out, 0x0a, 4, NULL, 0);
        break;
    case GPS_UBLOX_PORT: {
        uint8_t port[20] = {1, 0, 0, 0, 0xd0, 8, 0, 0}; /* UART1, 8N1. */
        for (unsigned i = 0; i < 4; ++i) port[8 + i] = (uint8_t)(g->target_baud >> (8 * i));
        port[12] = port[14] = 3; /* UBX and NMEA input/output. */
        encode(out, 6, 0, port, sizeof port);
        advance(g, GPS_UBLOX_SWITCH);
        g->waiting = true;
        const unsigned baud = g->probe_index ? bauds[g->probe_index - 1] : g->target_baud;
        const unsigned drain_ms = (28u * 10u * 1000u + baud - 1) / baud + 50u;
        g->deadline_ms = now + (drain_ms > SWITCH_MS ? drain_ms : SWITCH_MS);
        return true;
    }
    case GPS_UBLOX_CHECK_PORT:
        out->baud = g->target_baud;
        encode(out, 6, 0, &port_id, 1);
        break;
    case GPS_UBLOX_RATE: encode(out, 6, 8, rate, sizeof rate); break;
    case GPS_UBLOX_RMC: encode(out, 6, 1, rmc, sizeof rmc); break;
    case GPS_UBLOX_CHECK_RATE: encode(out, 6, 8, NULL, 0); break;
    case GPS_UBLOX_CHECK_RMC: encode(out, 6, 1, rmc, 2); break;
    default: return false;
    }
    g->waiting = true;
    ++g->attempts;
    if (out->baud) g->current_baud = out->baud;
    const unsigned timeout = frame_timeout(g) + 100u;
    g->deadline_ms = now + (timeout > RESPONSE_MS ? timeout : RESPONSE_MS);
    return true;
}
