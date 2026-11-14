/* MIDI message building / parsing (spec section 6). Pure logic, no hardware. */
#ifndef MIDI_H
#define MIDI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MIDI_PROGRAM_CHANGE_LEN  2
#define MIDI_SYSEX_POT_LEN       10
#define MIDI_PEDAL_REQUEST_LEN   11
#define MIDI_PEDAL_CHANGE_LEN    11
#define MIDI_PEDAL_RESPONSE_LEN  59

/* Roland checksum: (0x80 - (sum & 0x7F)) & 0x7F */
uint8_t midi_roland_checksum(const uint8_t *data, size_t len);

/* Each builder writes into 'out' (must be large enough) and returns the length. */
size_t midi_build_program_change(uint8_t *out, uint8_t channel, uint8_t group, uint8_t bank, uint8_t swt);
size_t midi_build_sysex_pot(uint8_t *out, uint8_t channel, uint8_t pot_value);
size_t midi_build_pedal_request(uint8_t *out, uint8_t channel);
/* bits: bit (n-1) is b_n */
size_t midi_build_pedal_change(uint8_t *out, uint8_t channel, uint8_t bits);

/* Incremental parser for MIDI_Sysex_Pedal_Response */
typedef struct {
    uint8_t buf[MIDI_PEDAL_RESPONSE_LEN];
    uint8_t n;
    bool    active;
} pedal_rx_t;

void pedal_rx_reset(pedal_rx_t *r);
/* Feed one received byte. Returns true when a complete, valid response for
 * 'channel' has been received; *bits_out then holds b1..b8 (bit n-1 = b_n). */
bool pedal_rx_feed(pedal_rx_t *r, uint8_t byte, uint8_t channel, uint8_t *bits_out);

#endif
