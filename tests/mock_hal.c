#include "mock_hal.h"
#include "hal.h"
#include "app.h"
#include <string.h>

uint32_t mock_now;
uint16_t mock_sw, mock_adc;
uint8_t  mock_tx[4096];
size_t   mock_tx_len;
bool     mock_led[12];
uint8_t  mock_seg;
uint8_t  mock_flash_channel;
int      mock_flash_writes;
bool     mock_flash_valid;
static uint8_t mock_rx[4096];
static size_t  rx_head, rx_tail;

void mock_reset(void)
{
    mock_now = 0; mock_sw = 0; mock_adc = 2000;
    mock_tx_len = 0; memset(mock_led, 0, sizeof mock_led); mock_seg = 0;
    rx_head = rx_tail = 0;
}
void mock_run(unsigned ms) { while (ms--) { mock_now++; app_task(); } }
void mock_push_rx(const uint8_t *d, size_t n) { while (n--) mock_rx[rx_head++] = *d++; }
bool mock_tx_is(const uint8_t *e, size_t n)
{
    bool ok = (mock_tx_len == n) && memcmp(mock_tx, e, n) == 0;
    mock_tx_len = 0;
    return ok;
}

void     hal_init(void) {}
uint32_t hal_millis(void) { return mock_now; }
uint16_t hal_switches_pressed(void) { return mock_sw; }
uint16_t hal_adc_read(void) { return mock_adc; }
void     hal_led_set(uint8_t n, bool on) { mock_led[n] = on; }
void     hal_segments_write(uint8_t b) { mock_seg = b; }
bool     hal_uart_send(const uint8_t *d, size_t n) { memcpy(mock_tx + mock_tx_len, d, n); mock_tx_len += n; return true; }
bool     hal_uart_rx_pop(uint8_t *b) { if (rx_tail == rx_head) return false; *b = mock_rx[rx_tail++]; return true; }
bool     hal_uart_tx_idle(void) { return true; }
uint8_t  hal_flash_load_channel(void) { return mock_flash_valid ? mock_flash_channel : 0; }
void     hal_flash_save_channel(uint8_t c) { mock_flash_channel = c; mock_flash_valid = true; mock_flash_writes++; }
