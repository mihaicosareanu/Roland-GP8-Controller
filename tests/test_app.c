#include "check.h"
#include "mock_hal.h"
#include "app.h"
#include <string.h>

#define B(n) (1u << ((n) - 1))
#define TX_IS(...) do { const uint8_t e[] = { __VA_ARGS__ }; CHECK(mock_tx_is(e, sizeof e)); } while (0)

static void boot(uint16_t held, uint8_t stored_channel, bool stored_valid)
{
    mock_reset();
    mock_flash_channel = stored_channel; mock_flash_valid = stored_valid; mock_flash_writes = 0;
    mock_sw = held;
    app_init();
    mock_run(40);
}
static void hold(int sw)      { mock_sw |= B(sw); mock_run(40); }
static void release_all(void) { mock_sw = 0;      mock_run(40); }
static void tap(int sw)       { hold(sw); release_all(); }
static bool leds_1_8_are(uint8_t mask)
{
    for (int n = 1; n <= 8; n++) if (mock_led[n] != ((mask >> (n - 1)) & 1)) return false;
    return true;
}

static void test_normal(void)
{
    boot(0, 0, false);
    CHECK(app_mode() == APP_NORMAL);
    CHECK(app_channel() == 0 && app_group() == 0 && app_bank() == 1 && app_swt() == 1);
    CHECK(leds_1_8_are(B(1)));                       /* default patch 1 */
    CHECK(mock_led[10] && !mock_led[11] && !mock_led[9]);
    CHECK(mock_seg == 0x06);                         /* digit 1, DP off */
    CHECK(mock_tx_len == 0);                         /* nothing sent at power-up */

    tap(3);                                          /* patch 3: 0*64 + 8*1 + 3 - 9 = 2 */
    TX_IS(0xC0, 0x02);
    CHECK(app_swt() == 3 && leds_1_8_are(B(3)));
    tap(3);                                          /* same patch again: sent again */
    TX_IS(0xC0, 0x02);

    tap(9);                                          /* group 1: 64 + 8 + 3 - 9 = 66 */
    TX_IS(0xC0, 0x42);
    CHECK(app_group() == 1 && !mock_led[10] && mock_led[11]);

    hold(10);                                        /* BANK_SELECT */
    CHECK(app_mode() == APP_BANK_SELECT && mock_seg == (0x06 | 0x80));
    release_all();
    tap(9); tap(11);                                 /* ignored in BANK_SELECT */
    CHECK(app_mode() == APP_BANK_SELECT && mock_tx_len == 0);
    tap(5);                                          /* bank 5: 64 + 40 + 3 - 9 = 98 */
    TX_IS(0xC0, 0x62);
    CHECK(app_mode() == APP_NORMAL && app_bank() == 5 && mock_seg == 0x6D);

    tap(10); tap(10);                                /* enter and cancel BANK_SELECT */
    CHECK(app_mode() == APP_NORMAL && app_bank() == 5 && mock_tx_len == 0 && mock_seg == 0x6D);

    /* two switches at once: first wins, second ignored */
    hold(2); mock_sw |= B(4); mock_run(40);
    TX_IS(0xC0, 0x00 + 0x40 + 0x28 + 2 - 9);         /* Group1 Bank5 SWT2 = 97 */
    release_all();
}

static void test_pot(void)
{
    boot(0, 0, false);
    mock_adc = 2000 + 31; mock_run(5);               /* below one step: nothing */
    CHECK(mock_tx_len == 0);
    mock_adc = 2000 + 32; mock_run(2);               /* 2032 >> 5 = 63 = 0x3F, checksum 0x24 */
    TX_IS(0xF0,0x41,0x00,0x13,0x12,0x00,0x1D,0x3F,0x24,0xF7);
    mock_adc = 2000 + 32 + 10; mock_run(2);
    CHECK(mock_tx_len == 0);

    /* ADC_old is re-initialized when NORMAL_MODE resumes, no message */
    hold(10); release_all();                         /* enter BANK_SELECT */
    mock_adc = 4000; mock_run(5);
    CHECK(mock_tx_len == 0);                         /* pot ignored in BANK_SELECT */
    tap(10);                                         /* leave */
    mock_run(5);
    CHECK(mock_tx_len == 0);                         /* 4000 became the new reference */
}

static void test_program_mode(void)
{
    boot(B(1) | B(2), 0, false);                     /* SW_1 + SW_2 held at power-up */
    CHECK(app_mode() == APP_PROG_ENTRY);
    CHECK(leds_1_8_are(0xFF) && mock_seg == 0x80 && !mock_led[9]);
    mock_run(250);
    CHECK(app_mode() == APP_PROG_WAIT && leds_1_8_are(0x00) && mock_seg == 0x80);

    mock_run(50);                                    /* SW_1/SW_2 still held: ignored */
    CHECK(app_mode() == APP_PROG_WAIT);
    release_all();
    tap(9); tap(11);                                 /* no effect */
    hold(10); release_all();                         /* lone SW_10: no effect */
    CHECK(app_mode() == APP_PROG_WAIT);

    hold(10); mock_sw |= B(3); mock_run(40);         /* SW_10 + SW_3 -> channel 10 */
    CHECK(app_mode() == APP_PROG_EXIT && app_channel() == 10);
    CHECK(mock_flash_channel == 10 && mock_flash_writes == 1);
    CHECK(mock_seg == 0x00);                         /* DP off */
    mock_run(100);
    release_all();
    mock_run(500);
    CHECK(app_mode() == APP_NORMAL);
    CHECK(mock_tx_len == 0);

    tap(1);                                          /* channel 10 = 0xCA */
    TX_IS(0xCA, 0x00);

    /* without SW_10: SW_n -> channel n-1 */
    boot(B(1) | B(2), 0, false);
    mock_run(250); release_all();
    tap(5);
    CHECK(app_channel() == 4 && mock_flash_channel == 4);

    /* stored channel is used at the next power-up */
    boot(0, 12, true);
    tap(1);
    TX_IS(0xCC, 0x00);
}

static void test_held_at_powerup(void)
{
    boot(B(3), 0, false);                            /* SW_3 held: ignored until released */
    CHECK(app_mode() == APP_NORMAL);
    mock_run(100);
    CHECK(mock_tx_len == 0);
    release_all();
    tap(4);
    TX_IS(0xC0, 0x03);                               /* 8 + 4 - 9 = 3 */
}

static const uint8_t RESP[59] = {
    0xF0,0x41,0x00,0x13,0x12,0x00,0x00,0x01,0x08,0x34,0x32,0x33,0x64,0x31,0x31,0x21,0x35,0x00,0x64,0x00,
    0x32,0x31,0x30,0x32,0x32,0x30,0x32,0x1F,0x03,0x28,0x21,0x01,0x24,0x64,0x28,0x2B,0x47,0x1B,0x00,0x00,
    0x20,0x20,0x20,0x20,0x20,0x42,0x59,0x50,0x41,0x53,0x53,0x20,0x20,0x20,0x20,0x20,0x00,0x4A,0xF7 };

static void test_pedal_mode(void)
{
    boot(0, 0, false);
    tap(3);                                          /* SWT = 3, remembered across PEDAL_MODE */
    mock_tx_len = 0;

    hold(11);
    CHECK(app_mode() == APP_PEDAL_WAIT);
    TX_IS(0xF0,0x41,0x00,0x13,0x11,0x00,0x00,0x00,0x32,0x4E,0xF7);
    CHECK(mock_led[9] && mock_seg == 0x73 && leds_1_8_are(0));
    mock_push_rx(RESP, sizeof RESP);
    mock_run(3);
    CHECK(app_mode() == APP_PEDAL && app_pedal_bits() == 0x18);
    CHECK(leds_1_8_are(0x18));                       /* LED_4 and LED_5 */
    release_all();

    mock_adc = 2000 + 64; mock_run(2);               /* pot works in PEDAL_MODE: 2064 >> 5 = 64 = 0x40 */
    TX_IS(0xF0,0x41,0x00,0x13,0x12,0x00,0x1D,0x40,0x23,0xF7);

    tap(9); tap(10);                                 /* ignored */
    CHECK(mock_tx_len == 0);

    tap(8);                                          /* b8 -> 1 */
    TX_IS(0xF0,0x41,0x00,0x13,0x12,0x00,0x00,0x09,0x08,0x6F,0xF7);
    CHECK(leds_1_8_are(0x98));
    tap(7);
    TX_IS(0xF0,0x41,0x00,0x13,0x12,0x00,0x00,0x0D,0x08,0x6B,0xF7);
    CHECK(leds_1_8_are(0xD8));
    tap(8);                                          /* toggles back */
    TX_IS(0xF0,0x41,0x00,0x13,0x12,0x00,0x00,0x05,0x08,0x73,0xF7);
    CHECK(leds_1_8_are(0x58));

    tap(11);                                         /* leave: Program Change for the remembered patch */
    TX_IS(0xC0, 0x02);                               /* group 0, bank 1, SWT 3 */
    CHECK(app_mode() == APP_NORMAL && app_pedal_bits() == 0);
    CHECK(!mock_led[9] && leds_1_8_are(B(3)) && mock_seg == 0x06);
}

static void test_pedal_timeout_and_filtering(void)
{
    boot(0, 0, false);
    hold(11);                                        /* request sent, window running */
    mock_tx_len = 0;
    release_all();                                   /* 40 ms elapsed in the window */
    CHECK(app_mode() == APP_PEDAL_WAIT);
    mock_adc = 3500; mock_sw = 0; mock_run(5);       /* pot ignored inside the window */
    CHECK(mock_tx_len == 0);
    mock_run(100);                                   /* > 120 ms since the window started: timeout */
    CHECK(app_mode() == APP_PEDAL && app_pedal_bits() == 0 && leds_1_8_are(0));
    mock_run(5);                                     /* ADC_old was not updated, so pot now reports */
    CHECK(mock_tx_len == 10);
    mock_tx_len = 0;

    /* first press after a timeout toggles from all zeros */
    tap(1);
    TX_IS(0xF0,0x41,0x00,0x13,0x12,0x00,0x00,0x00,0x01,0x7F,0xF7);

    /* a response for another channel / with a bad checksum is rejected and the window times out */
    boot(0, 0, false);
    uint8_t bad[59]; memcpy(bad, RESP, 59); bad[57] ^= 0x01;
    hold(11); release_all();
    mock_tx_len = 0;
    boot(0, 0, false);
    hold(11);
    mock_push_rx(bad, sizeof bad);
    mock_run(5);
    CHECK(app_mode() == APP_PEDAL_WAIT && app_pedal_bits() == 0);
    mock_run(130);
    CHECK(app_mode() == APP_PEDAL && app_pedal_bits() == 0);
}

int main(void)
{
    test_normal();
    test_pot();
    test_program_mode();
    test_held_at_powerup();
    test_pedal_mode();
    test_pedal_timeout_and_filtering();
    REPORT("test_app");
}
