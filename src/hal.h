/* Hardware abstraction used by the application logic.
 * Implemented by hal_stm32.c on the target and by tests/mock_hal.c on the host. */
#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void     hal_init(void);
uint32_t hal_millis(void);

/* Bit (n-1) set => SW_n is pressed (pin LOW). n = 1..11 */
uint16_t hal_switches_pressed(void);

/* Raw 12-bit reading of POT (0..4095) */
uint16_t hal_adc_read(void);

/* n = 1..11 selects LED_n. HIGH = ON. */
void     hal_led_set(uint8_t n, bool on);

/* bit0..bit6 = segments A..G, bit7 = DP. 1 = segment ON. */
void     hal_segments_write(uint8_t bits);

/* Queue bytes for transmission (non-blocking). Returns false if they do not fit. */
bool     hal_uart_send(const uint8_t *data, size_t len);
/* Pop one received byte, if any. */
bool     hal_uart_rx_pop(uint8_t *byte);
/* True when everything queued has been fully shifted out on the wire. */
bool     hal_uart_tx_idle(void);

/* MIDI_Channel persistence. Load returns 0 if flash is empty or invalid. */
uint8_t  hal_flash_load_channel(void);
void     hal_flash_save_channel(uint8_t channel);

#endif
