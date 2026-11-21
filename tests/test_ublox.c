#include "gps_ublox.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static gps_ublox_t gps;
static gps_ublox_action_t action;
static uint64_t now;
static unsigned byte_ms;

/* Independent receiver-side encoder, including checksum corruption. */
static void reply(unsigned cls, unsigned id, const uint8_t *p, size_t n, bool corrupt)
{
    uint8_t wire[520] = {0xb5, 0x62, (uint8_t)cls, (uint8_t)id, (uint8_t)n, (uint8_t)(n >> 8)};
    assert(n <= sizeof wire - 8);
    if (n) memcpy(wire + 6, p, n);
    uint8_t a = 0, b = 0;
    for (size_t i = 2; i < n + 6; ++i) { a += wire[i]; b += a; }
    wire[n + 6] = a; wire[n + 7] = b ^ corrupt;
    for (size_t i = 0; i < n + 8; ++i) {
        assert(gps_ublox_feed(&gps, wire[i], now)); now += byte_ms;
    }
}
static void version(bool corrupt)
{
    uint8_t p[70] = {0};
    memcpy(p, "7.03 (45969)", 12); memcpy(p + 30, "00040007", 8);
    reply(0x0a, 4, p, sizeof p, corrupt);
}
static void start(void) { now = 0; byte_ms=0; gps_ublox_init(&gps, 9600); }
static void command(unsigned cls, unsigned id, size_t n)
{
    assert(gps_ublox_next(&gps, now, &action));
    assert(action.size == n + 8 && action.bytes[0] == 0xb5 && action.bytes[1] == 0x62);
    assert(action.bytes[2] == cls && action.bytes[3] == id);
    uint8_t a = 0, b = 0;
    for (size_t i = 2; i < n + 6; ++i) { a += action.bytes[i]; b += a; }
    assert(a == action.bytes[n + 6] && b == action.bytes[n + 7]);
}
static void ack(unsigned id, bool yes)
{ uint8_t p[] = {6, (uint8_t)id}; reply(5, yes ? 1 : 0, p, 2, false); }
static void port(void)
{
    const uint8_t p[20] = {1,0,0,0,0xd0,8,0,0,0x80,0x25,0,0,3,0,3,0,0,0,0,0};
    reply(6, 0, p, sizeof p, false);
}
static void through_port(void)
{
    command(0x0a, 4, 0); version(false);
    assert(gps.detected && strcmp(gps.hardware, "00040007") == 0);
    command(6, 0, 20);
    assert(action.bytes[6] == 1 && action.bytes[10] == 0xd0 && action.bytes[11] == 8);
    assert(action.bytes[14] == 0x80 && action.bytes[15] == 0x25);
    assert(action.bytes[18] == 3 && action.bytes[20] == 3);
    assert(!gps_ublox_next(&gps, now, &action));
    now += 150; command(6, 0, 1); assert(action.baud == 9600);
    port(); command(6, 8, 6);
    assert(memcmp(action.bytes + 6, (uint8_t[]){0xe8,3,1,0,1,0}, 6) == 0);
}

int main(void)
{
    start(); through_port();
    ack(0, true); assert(!gps_ublox_next(&gps, now, &action)); /* unrelated ACK */
    ack(8, true); command(6, 1, 3);
    assert(memcmp(action.bytes + 6, (uint8_t[]){0xf0,4,1}, 3) == 0);
    ack(1, true); command(6, 8, 0);
    const uint8_t rate[] = {0xe8,3,1,0,1,0}; reply(6,8,rate,6,false);
    command(6,1,2);
    const uint8_t msg[] = {0xf0,4,0,1,0,0,0,0}; reply(6,1,msg,8,false);
    assert(gps.phase == GPS_UBLOX_READY && !gps_ublox_next(&gps, now, &action));

    start(); command(0x0a,4,0); version(true);
    assert(!gps.detected); now = 1200; command(0x0a,4,0); assert(action.baud == 4800);
    version(false); assert(gps.detected); /* Binary-only receiver at another baud. */

    start(); through_port(); ack(8,false);
    assert(gps.phase == GPS_UBLOX_FALLBACK);
    assert(gps_ublox_next(&gps,now,&action) && action.baud == 9600 && action.size == 0);
    assert(!gps_ublox_next(&gps,now + 60000,&action));

    start(); through_port(); now += 1200; command(6,8,6);
    now += 1200; assert(gps_ublox_next(&gps,now,&action));
    assert(gps.phase == GPS_UBLOX_FALLBACK && action.size == 0);

    start();
    const unsigned bauds[] = {9600,4800,19200,38400,57600,115200,230400};
    for (size_t i = 0; i < sizeof bauds / sizeof *bauds; ++i) {
        command(0x0a,4,0); assert(action.baud == bauds[i]); now += 1200;
    }
    assert(gps_ublox_next(&gps,now,&action) && gps.phase == GPS_UBLOX_FALLBACK);
    assert(action.baud == 9600 && action.size == 0);

    start(); command(0x0a,4,0);
    uint8_t junk[300]; memset(junk, '$', sizeof junk);
    reply(1,7,junk,sizeof junk,false); assert(!gps.detected);
    version(false); assert(gps.detected); /* Oversized frame drains safely. */
    gps_ublox_rx_reset(&gps);
    assert(gps_ublox_feed(&gps,0xb5,now)); assert(gps_ublox_feed(&gps,0x62,now));
    now += 501; assert(!gps_ublox_feed(&gps,'$',now)); /* Partial-frame timeout. */

    start(); command(0x0a,4,0);
    uint8_t v7[40] = {0};
    memcpy(v7,"1.00 (59842)",12); memcpy(v7+30,"00070000",8);
    reply(0x0a,4,v7,40,false); /* u-blox 7 format, used by compatible GT-U7s. */
    assert(gps.detected && strcmp(gps.hardware,"00070000") == 0);

    start(); through_port(); ack(8,true); command(6,1,3); ack(1,true); command(6,8,0);
    const uint8_t wrong_rate[] = {200,0,1,0,1,0}; reply(6,8,wrong_rate,6,false);
    assert(gps.phase == GPS_UBLOX_FALLBACK); /* ACK alone cannot establish success. */

    start(); command(0x0a,4,0);
    uint8_t malformed[40]; memset(malformed,'x',sizeof malformed);
    reply(0x0a,4,malformed,40,false); assert(!gps.detected);
    const char *sentence = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";
    reply(1,7,(const uint8_t *)sentence,strlen(sentence),false);
    assert(!gps.detected); /* Embedded text is fully consumed as binary. */

    now = 0; gps_ublox_init(&gps,38400);
    const unsigned custom[] = {38400,4800,9600,19200,57600,115200,230400};
    for (size_t i=0; i<sizeof custom/sizeof *custom; ++i) {
        command(0x0a,4,0); assert(action.baud == custom[i]); now += 1200;
    }
    assert(gps_ublox_next(&gps,now,&action) && action.baud == 38400);

    now=0; gps_ublox_init(&gps,1200); command(0x0a,4,0); version(false);
    command(6,0,20); now=150;
    assert(!gps_ublox_next(&gps,now,&action)); /* 28-byte command still transmitting. */
    now=300; command(6,0,1); assert(action.baud == 1200);

    now=0; gps_ublox_init(&gps,1200); command(0x0a,4,0);
    byte_ms=9; version(false); assert(gps.detected); /* Serialized 1200-baud reply. */
    puts("PASS: UBX discovery, baud recovery, ACK/readback, corruption, timeout and fallback");
}
