/* STM32F401RCT6 hardware layer, bare CMSIS register access (no HAL library).
 * Clock: internal HSI 16 MHz, no PLL (board independent, USART1 31250 baud is exact). */

#ifndef STM32F401xC
#define STM32F401xC
#endif
#include "stm32f4xx.h"
#include "hal.h"

/* ---------- pin map (keep in sync with schematic.md section 2) ---------- */

typedef struct { GPIO_TypeDef *port; uint8_t pin; } pin_t;

static const pin_t SEG_PINS[8] = {            /* SEG_A..SEG_G, SEG_DP */
    {GPIOA, 0}, {GPIOA, 1}, {GPIOA, 2}, {GPIOA, 3},
    {GPIOA, 4}, {GPIOA, 5}, {GPIOA, 6}, {GPIOA, 7}
};

static const pin_t LED_PINS[11] = {           /* LED_1..LED_11 (MOSFET gates) */
    {GPIOA, 8},  {GPIOA, 9},  {GPIOA, 10}, {GPIOA, 11},
    {GPIOA, 12}, {GPIOA, 13}, {GPIOA, 14}, {GPIOA, 15},
    {GPIOB, 15}, {GPIOC, 13}, {GPIOC, 14}
};

static const pin_t SW_PINS[11] = {            /* SW_1..SW_11 (active LOW, external pull-ups) */
    {GPIOB, 1},  {GPIOB, 2},  {GPIOB, 3},  {GPIOB, 4},  {GPIOB, 5},
    {GPIOB, 8},  {GPIOB, 9},  {GPIOB, 10}, {GPIOB, 12}, {GPIOB, 13}, {GPIOB, 14}
};

#define POT_ADC_CHANNEL 8                     /* PB0 = ADC1_IN8 */
#define MIDI_BAUD       31250u                /* USART1: PB6 TX, PB7 RX (AF7) */

/* MIDI_Channel storage: flash sector 3 (16 KB at 0x0800C000) */
#define CH_FLASH_ADDR   0x0800C000u
#define CH_FLASH_SECTOR 3u
#define CH_MAGIC        0xA5A50000u

/* ---------- GPIO helpers ---------- */

enum { MODE_IN = 0, MODE_OUT = 1, MODE_AF = 2, MODE_ANALOG = 3 };

static void gpio_config(GPIO_TypeDef *p, uint8_t pin, uint32_t mode, uint32_t pull)
{
    p->PUPDR = (p->PUPDR & ~(3u << (pin * 2))) | (pull << (pin * 2));
    p->MODER = (p->MODER & ~(3u << (pin * 2))) | (mode << (pin * 2));
}

static void gpio_output_low(const pin_t *pp)
{
    pp->port->BSRR = 1u << (pp->pin + 16);    /* drive LOW before enabling the output */
    gpio_config(pp->port, pp->pin, MODE_OUT, 0);
    pp->port->OTYPER &= ~(1u << pp->pin);     /* push-pull */
}

static void gpio_af(GPIO_TypeDef *p, uint8_t pin, uint32_t af)
{
    uint32_t idx = pin >> 3, sh = (pin & 7u) * 4u;
    p->AFR[idx] = (p->AFR[idx] & ~(0xFu << sh)) | (af << sh);
}

/* ---------- time ---------- */

static volatile uint32_t ticks;

void SysTick_Handler(void) { ticks++; }
uint32_t hal_millis(void)  { return ticks; }

/* ---------- UART (interrupt driven, ring buffers) ---------- */

#define TX_SIZE 256u
#define RX_SIZE 128u
static volatile uint8_t  tx_buf[TX_SIZE];
static volatile uint16_t tx_head, tx_tail;
static volatile uint8_t  rx_buf[RX_SIZE];
static volatile uint16_t rx_head, rx_tail;

void USART1_IRQHandler(void)
{
    uint32_t sr = USART1->SR;

    if (sr & (USART_SR_RXNE | USART_SR_ORE)) {
        uint8_t d = (uint8_t)USART1->DR;      /* SR read + DR read clears the flags */
        uint16_t next = (uint16_t)((rx_head + 1u) % RX_SIZE);
        if (next != rx_tail) {
            rx_buf[rx_head] = d;
            rx_head = next;
        }
    }
    if ((sr & USART_SR_TXE) && (USART1->CR1 & USART_CR1_TXEIE)) {
        if (tx_tail != tx_head) {
            USART1->DR = tx_buf[tx_tail];
            tx_tail = (uint16_t)((tx_tail + 1u) % TX_SIZE);
        } else {
            USART1->CR1 &= ~USART_CR1_TXEIE;
        }
    }
}

bool hal_uart_send(const uint8_t *data, size_t len)
{
    uint16_t space = (uint16_t)((tx_tail + TX_SIZE - tx_head - 1u) % TX_SIZE);
    if (len > space) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        tx_buf[tx_head] = data[i];
        tx_head = (uint16_t)((tx_head + 1u) % TX_SIZE);
    }
    USART1->CR1 |= USART_CR1_TXEIE;
    return true;
}

bool hal_uart_rx_pop(uint8_t *byte)
{
    if (rx_head == rx_tail) {
        return false;
    }
    *byte = rx_buf[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1u) % RX_SIZE);
    return true;
}

bool hal_uart_tx_idle(void)
{
    return (tx_tail == tx_head) && (USART1->SR & USART_SR_TC);
}

/* ---------- init ---------- */

void hal_init(void)
{
    SystemCoreClockUpdate();

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN | RCC_APB2ENR_USART1EN;
    (void)RCC->AHB1ENR;                       /* read-back: let the clocks settle */

    /* PA13/PA14 (SWD), PA15/PB3/PB4 (JTAG) become plain GPIO here. */
    for (int i = 0; i < 8; i++)  gpio_output_low(&SEG_PINS[i]);
    for (int i = 0; i < 11; i++) gpio_output_low(&LED_PINS[i]);
    for (int i = 0; i < 11; i++) gpio_config(SW_PINS[i].port, SW_PINS[i].pin, MODE_IN, 0);

    /* POT: PB0 analog */
    gpio_config(GPIOB, 0, MODE_ANALOG, 0);
    ADC1->SMPR2 = (7u << (3u * POT_ADC_CHANNEL));   /* 480 cycles sample time */
    ADC1->SQR1  = 0;                                /* one conversion */
    ADC1->SQR3  = POT_ADC_CHANNEL;
    ADC1->CR2   = ADC_CR2_ADON;                     /* 12-bit, right aligned, software trigger */

    /* USART1 on PB6 (TX) / PB7 (RX), AF7. Internal pull-up on RX keeps it idle-high if unconnected. */
    gpio_config(GPIOB, 6, MODE_AF, 0);
    gpio_config(GPIOB, 7, MODE_AF, 1);
    gpio_af(GPIOB, 6, 7);
    gpio_af(GPIOB, 7, 7);
    USART1->BRR = SystemCoreClock / MIDI_BAUD;      /* APB2 = SYSCLK = 16 MHz: exactly 512 */
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE;
    NVIC_EnableIRQ(USART1_IRQn);

    SysTick_Config(SystemCoreClock / 1000u);        /* 1 ms tick */
}

/* ---------- switches / LEDs / display / ADC ---------- */

uint16_t hal_switches_pressed(void)
{
    uint16_t mask = 0;
    for (int i = 0; i < 11; i++) {
        if ((SW_PINS[i].port->IDR & (1u << SW_PINS[i].pin)) == 0) {   /* active LOW */
            mask |= (uint16_t)(1u << i);
        }
    }
    return mask;
}

void hal_led_set(uint8_t n, bool on)
{
    const pin_t *pp;
    if (n < 1 || n > 11) {
        return;
    }
    pp = &LED_PINS[n - 1];
    pp->port->BSRR = on ? (1u << pp->pin) : (1u << (pp->pin + 16));   /* HIGH = ON */
}

void hal_segments_write(uint8_t bits)
{
    /* SEG_A..SEG_DP are PA0..PA7: one atomic write, HIGH = ON */
    GPIOA->BSRR = (uint32_t)bits | ((uint32_t)(uint8_t)~bits << 16);
}

uint16_t hal_adc_read(void)
{
    ADC1->SR = 0;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while (!(ADC1->SR & ADC_SR_EOC)) { }
    return (uint16_t)(ADC1->DR & 0x0FFFu);
}

/* ---------- flash (MIDI_Channel) ---------- */

static void flash_wait(void) { while (FLASH->SR & FLASH_SR_BSY) { } }

uint8_t hal_flash_load_channel(void)
{
    uint32_t w = *(volatile uint32_t *)CH_FLASH_ADDR;
    if ((w & 0xFFFF0000u) == CH_MAGIC && (w & 0xFFFFu) <= 15u) {
        return (uint8_t)(w & 0xFu);
    }
    return 0;
}

void hal_flash_save_channel(uint8_t channel)
{
    uint32_t stored = *(volatile uint32_t *)CH_FLASH_ADDR;
    if (stored == (CH_MAGIC | channel)) {
        return;                                      /* already stored: avoid a needless erase */
    }

    FLASH->KEYR = 0x45670123u;
    FLASH->KEYR = 0xCDEF89ABu;
    flash_wait();
    FLASH->SR = FLASH_SR_EOP | FLASH_SR_OPERR | FLASH_SR_WRPERR |
                FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR;

    FLASH->CR = FLASH_CR_SER | (CH_FLASH_SECTOR << FLASH_CR_SNB_Pos) | FLASH_CR_PSIZE_1;
    FLASH->CR |= FLASH_CR_STRT;                      /* erase sector 3 (CPU stalls while busy) */
    flash_wait();

    FLASH->CR = FLASH_CR_PG | FLASH_CR_PSIZE_1;      /* program one 32-bit word */
    *(volatile uint32_t *)CH_FLASH_ADDR = CH_MAGIC | channel;
    flash_wait();

    FLASH->CR = 0;
    FLASH->CR |= FLASH_CR_LOCK;
}
