#ifndef MOCK_HAL_H
#define MOCK_HAL_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

extern uint32_t mock_now;
extern uint16_t mock_sw;               /* pressed mask, bit (n-1) = SW_n */
extern uint16_t mock_adc;
extern uint8_t  mock_tx[4096];
extern size_t   mock_tx_len;
extern bool     mock_led[12];          /* index 1..11 */
extern uint8_t  mock_seg;
extern uint8_t  mock_flash_channel;
extern int      mock_flash_writes;
extern bool     mock_flash_valid;

void mock_reset(void);
void mock_run(unsigned ms);            /* advance time 1 ms at a time, calling app_task() */
void mock_push_rx(const uint8_t *d, size_t n);
bool mock_tx_is(const uint8_t *expected, size_t n);   /* compares whole log, then clears it */
#endif
