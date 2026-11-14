#include "check.h"
#include "midi.h"
#include <string.h>

static void expect(const uint8_t *got, size_t n, const uint8_t *want, size_t m)
{
    CHECK(n == m && memcmp(got, want, m) == 0);
}

int main(void)
{
    uint8_t m[64];
    size_t n;

    /* 6.1 Program Change examples */
    n = midi_build_program_change(m, 0, 0, 1, 1);   expect(m, n, (uint8_t[]){0xC0,0x00}, 2);
    n = midi_build_program_change(m, 0, 0, 2, 3);   expect(m, n, (uint8_t[]){0xC0,0x0A}, 2);
    n = midi_build_program_change(m, 3, 1, 1, 1);   expect(m, n, (uint8_t[]){0xC3,0x40}, 2);
    n = midi_build_program_change(m, 15, 1, 8, 8);  expect(m, n, (uint8_t[]){0xCF,0x7F}, 2);

    /* 6.2 SysEx pot test vectors */
    n = midi_build_sysex_pot(m, 0, 0x00); expect(m, n, (uint8_t[]){0xF0,0x41,0x00,0x13,0x12,0x00,0x1D,0x00,0x63,0xF7}, 10);
    n = midi_build_sysex_pot(m, 0, 0x40); expect(m, n, (uint8_t[]){0xF0,0x41,0x00,0x13,0x12,0x00,0x1D,0x40,0x23,0xF7}, 10);
    n = midi_build_sysex_pot(m, 0, 0x63); expect(m, n, (uint8_t[]){0xF0,0x41,0x00,0x13,0x12,0x00,0x1D,0x63,0x00,0xF7}, 10);
    n = midi_build_sysex_pot(m, 0, 0x7F); expect(m, n, (uint8_t[]){0xF0,0x41,0x00,0x13,0x12,0x00,0x1D,0x7F,0x64,0xF7}, 10);
    n = midi_build_sysex_pot(m, 5, 0x10); CHECK(m[2] == 5);

    /* 6.4 request */
    n = midi_build_pedal_request(m, 0);
    expect(m, n, (uint8_t[]){0xF0,0x41,0x00,0x13,0x11,0x00,0x00,0x00,0x32,0x4E,0xF7}, 11);

    /* 6.6 pedal change test vectors */
    n = midi_build_pedal_change(m, 0, 0x00); expect(m, n, (uint8_t[]){0xF0,0x41,0x00,0x13,0x12,0x00,0x00,0x00,0x00,0x00,0xF7}, 11);
    n = midi_build_pedal_change(m, 0, 0x98); expect(m, n, (uint8_t[]){0xF0,0x41,0x00,0x13,0x12,0x00,0x00,0x09,0x08,0x6F,0xF7}, 11);
    n = midi_build_pedal_change(m, 0, 0xD8); expect(m, n, (uint8_t[]){0xF0,0x41,0x00,0x13,0x12,0x00,0x00,0x0D,0x08,0x6B,0xF7}, 11);
    n = midi_build_pedal_change(m, 0, 0xFF); expect(m, n, (uint8_t[]){0xF0,0x41,0x00,0x13,0x12,0x00,0x00,0x0F,0x0F,0x62,0xF7}, 11);

    /* 6.5 response parsing: real capture from the spec (channel 0) */
    static const uint8_t resp[59] = {
        0xF0,0x41,0x00,0x13,0x12,0x00,0x00,0x01,0x08,0x34,0x32,0x33,0x64,0x31,0x31,0x21,0x35,0x00,0x64,0x00,
        0x32,0x31,0x30,0x32,0x32,0x30,0x32,0x1F,0x03,0x28,0x21,0x01,0x24,0x64,0x28,0x2B,0x47,0x1B,0x00,0x00,
        0x20,0x20,0x20,0x20,0x20,0x42,0x59,0x50,0x41,0x53,0x53,0x20,0x20,0x20,0x20,0x20,0x00,0x4A,0xF7 };
    pedal_rx_t rx; uint8_t bits = 0xAA; bool got = false;

    pedal_rx_reset(&rx);
    for (int i = 0; i < 59; i++) { if (pedal_rx_feed(&rx, resp[i], 0, &bits)) got = true; }
    CHECK(got); CHECK(bits == 0x18);                 /* b8..b1 = 0001 1000 */

    /* wrong channel, bad checksum, truncated, too long, garbage before, real-time byte inside */
    got = false; pedal_rx_reset(&rx);
    for (int i = 0; i < 59; i++) { if (pedal_rx_feed(&rx, resp[i], 3, &bits)) got = true; }
    CHECK(!got);

    uint8_t bad[59]; memcpy(bad, resp, 59); bad[20] ^= 0x01;
    got = false; pedal_rx_reset(&rx);
    for (int i = 0; i < 59; i++) { if (pedal_rx_feed(&rx, bad[i], 0, &bits)) got = true; }
    CHECK(!got);

    got = false; pedal_rx_reset(&rx);
    for (int i = 0; i < 57; i++) { pedal_rx_feed(&rx, resp[i], 0, &bits); }
    if (pedal_rx_feed(&rx, 0xF7, 0, &bits)) got = true;   /* one byte short (58 bytes in total) */
    CHECK(!got);

    got = false; pedal_rx_reset(&rx);
    for (int i = 0; i < 58; i++) { pedal_rx_feed(&rx, resp[i], 0, &bits); }
    pedal_rx_feed(&rx, 0x20, 0, &bits);                   /* one byte too many */
    if (pedal_rx_feed(&rx, 0xF7, 0, &bits)) got = true;
    CHECK(!got);

    got = false; pedal_rx_reset(&rx);
    pedal_rx_feed(&rx, 0x55, 0, &bits); pedal_rx_feed(&rx, 0x12, 0, &bits);
    for (int i = 0; i < 59; i++) {
        if (i == 30) pedal_rx_feed(&rx, 0xF8, 0, &bits);  /* MIDI clock in the middle */
        if (pedal_rx_feed(&rx, resp[i], 0, &bits)) got = true;
    }
    CHECK(got);

    REPORT("test_midi");
}
