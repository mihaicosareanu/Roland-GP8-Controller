/* Application state machine (spec sections 2-7). Hardware-independent. */
#ifndef APP_H
#define APP_H

#include <stdint.h>

typedef enum {
    APP_STARTUP,       /* first 30 ms after power-up: decide PROGRAM_MODE or NORMAL_MODE */
    APP_PROG_ENTRY,    /* PROGRAM_MODE, entry blink running */
    APP_PROG_WAIT,     /* PROGRAM_MODE, waiting for channel selection */
    APP_PROG_EXIT,     /* PROGRAM_MODE, exit blinks running */
    APP_NORMAL,
    APP_BANK_SELECT,
    APP_PEDAL_WAIT,    /* PEDAL_MODE, waiting for the pedal response (max 120 ms) */
    APP_PEDAL
} app_mode_t;

void app_init(void);   /* call once after hal_init() */
void app_task(void);   /* call continuously from the main loop */

/* Read-only state, mainly for tests */
app_mode_t app_mode(void);
uint8_t app_channel(void);
uint8_t app_group(void);
uint8_t app_bank(void);
uint8_t app_swt(void);
uint8_t app_pedal_bits(void);

#endif
