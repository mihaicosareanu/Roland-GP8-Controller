#include "midi.h"

uint8_t midi_roland_checksum(const uint8_t *data, size_t len)
{
    uint32_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return (uint8_t)((0x80u - (sum & 0x7Fu)) & 0x7Fu);
}

size_t midi_build_program_change(uint8_t *out, uint8_t channel, uint8_t group, uint8_t bank, uint8_t swt)
{
    int program = 64 * group + 8 * bank + swt - 9;
    out[0] = (uint8_t)(0xC0u | (channel & 0x0Fu));
    out[1] = (uint8_t)(program & 0x7F);
    return MIDI_PROGRAM_CHANGE_LEN;
}

size_t midi_build_sysex_pot(uint8_t *out, uint8_t channel, uint8_t pot_value)
{
    uint8_t bb = pot_value & 0x7Fu;
    uint8_t addr_data[3] = { 0x00, 0x1D, bb };
    out[0] = 0xF0; out[1] = 0x41; out[2] = channel & 0x0Fu;
    out[3] = 0x13; out[4] = 0x12; out[5] = 0x00; out[6] = 0x1D;
    out[7] = bb;
    out[8] = midi_roland_checksum(addr_data, sizeof addr_data);
    out[9] = 0xF7;
    return MIDI_SYSEX_POT_LEN;
}

size_t midi_build_pedal_request(uint8_t *out, uint8_t channel)
{
    out[0] = 0xF0; out[1] = 0x41; out[2] = channel & 0x0Fu;
    out[3] = 0x13; out[4] = 0x11; out[5] = 0x00; out[6] = 0x00;
    out[7] = 0x00; out[8] = 0x32; out[9] = 0x4E; out[10] = 0xF7;
    return MIDI_PEDAL_REQUEST_LEN;
}

size_t midi_build_pedal_change(uint8_t *out, uint8_t channel, uint8_t bits)
{
    uint8_t bb = (uint8_t)((bits >> 4) & 0x0Fu);
    uint8_t cc = (uint8_t)(bits & 0x0Fu);
    uint8_t chk_data[4] = { 0x00, 0x00, bb, cc };
    out[0] = 0xF0; out[1] = 0x41; out[2] = channel & 0x0Fu;
    out[3] = 0x13; out[4] = 0x12; out[5] = 0x00; out[6] = 0x00;
    out[7] = bb; out[8] = cc;
    out[9] = midi_roland_checksum(chk_data, sizeof chk_data);
    out[10] = 0xF7;
    return MIDI_PEDAL_CHANGE_LEN;
}

void pedal_rx_reset(pedal_rx_t *r)
{
    r->n = 0;
    r->active = false;
}

static bool response_valid(const pedal_rx_t *r, uint8_t channel)
{
    static const uint8_t hdr[7] = { 0xF0, 0x41, 0x00, 0x13, 0x12, 0x00, 0x00 };
    for (int i = 0; i < 7; i++) {
        uint8_t expect = (i == 2) ? (uint8_t)(channel & 0x0Fu) : hdr[i];
        if (r->buf[i] != expect) {
            return false;
        }
    }
    /* checksum over bytes 6..57 (1-based) = indices 5..56, stored at index 57 */
    return midi_roland_checksum(&r->buf[5], 52) == r->buf[57];
}

bool pedal_rx_feed(pedal_rx_t *r, uint8_t byte, uint8_t channel, uint8_t *bits_out)
{
    if (byte >= 0xF8) {            /* real-time bytes may appear anywhere: ignore */
        return false;
    }
    if (byte == 0xF0) {            /* (re)start of a SysEx */
        r->active = true;
        r->n = 0;
        r->buf[r->n++] = byte;
        return false;
    }
    if (!r->active) {
        return false;
    }
    if (byte == 0xF7) {
        bool ok = false;
        if (r->n == MIDI_PEDAL_RESPONSE_LEN - 1) {
            r->buf[r->n++] = byte;
            ok = response_valid(r, channel);
            if (ok) {
                *bits_out = (uint8_t)(((r->buf[7] & 0x0Fu) << 4) | (r->buf[8] & 0x0Fu));
            }
        }
        pedal_rx_reset(r);
        return ok;
    }
    if ((byte & 0x80u) || r->n >= MIDI_PEDAL_RESPONSE_LEN - 1) {
        pedal_rx_reset(r);         /* unexpected status byte or too long: abort */
        return false;
    }
    r->buf[r->n++] = byte;
    return false;
}
