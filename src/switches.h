/* Switch debouncing and "first press wins" lockout (spec section 1.1). Pure logic. */
#ifndef SWITCHES_H
#define SWITCHES_H

#include <stdint.h>
#include <stdbool.h>

#define SW_COUNT    11
#define DEBOUNCE_MS 30u

typedef struct {
    uint16_t raw;                  /* last raw state, bit (n-1) = SW_n pressed */
    uint16_t deb;                  /* debounced state */
    uint32_t t_change[SW_COUNT];   /* time of last raw change per switch */
    bool     locked;               /* a switch was accepted; waiting for all released */
    uint8_t  accepted;             /* accepted switch number (1..11) */
    bool     second_used;          /* PROGRAM_MODE combination already used */
} switches_t;

typedef struct {
    uint8_t sw;                    /* accepted switch number 1..11 */
    bool    sw10_held;             /* true if accepted as 2nd switch of the SW_10 combo */
} sw_event_t;

/* Switches already pressed at 'init' time are ignored until released. */
void switches_init(switches_t *s, uint32_t now_ms, uint16_t pressed_mask);

/* Call regularly with the raw pressed mask. Returns true if a press was accepted. */
bool switches_update(switches_t *s, uint32_t now_ms, uint16_t pressed_mask,
                     bool combo_enabled, sw_event_t *ev);

#endif
