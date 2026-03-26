/*
 * ds1620.c — Dallas DS1620 3-wire thermometer driver for STM32F411 @ 100 MHz
 *
 * 3-wire protocol:
 *   RST HIGH  → enables DS1620 for communication
 *   RST LOW   → DS1620 ignores CLK/DQ
 *   Data is clocked LSB-first on the rising edge of CLK (write)
 *   or latched by DS1620 on the falling edge, read by MCU after rise (read).
 *   Minimum clock period: 500 ns (@ 5V), 1 µs is safe margin.
 *
 * DS1620 commands:
 *   0xAC — Write Config (1 byte)
 *   0xEE — Start Convert T (no data)
 *   0xAA — Read Temperature (9 bits → 2 bytes, LSB first)
 *
 * Config byte bit1 (CPU): 1 = CPU-driven mode (no standalone thermostat)
 * Config byte bit0 (1SHOT): 0 = continuous conversion
 */

#include "DS1620/ds1620.h"
#include "main.h"   /* stm32f4xx.h, delay_ms(), SYSTEM_CORE_CLOCK */

/* ── Pin definitions (GPIOB) ────────────────────────────────────── */
#define DS1620_PORT     GPIOB
#define DS1620_CLK_PIN  3U
#define DS1620_DQ_PIN   4U
#define DS1620_RST_PIN  5U

#define CLK_MASK   (1U << DS1620_CLK_PIN)
#define DQ_MASK    (1U << DS1620_DQ_PIN)
#define RST_MASK   (1U << DS1620_RST_PIN)

/* ── DWT µs delay ───────────────────────────────────────────────── */
static inline void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SYSTEM_CORE_CLOCK / 1000000U);
    while ((DWT->CYCCNT - start) < ticks);
}

/* ── GPIO helpers ───────────────────────────────────────────────── */
static inline void clk_high(void) { DS1620_PORT->BSRR = CLK_MASK; }
static inline void clk_low(void)  { DS1620_PORT->BSRR = CLK_MASK << 16U; }
static inline void rst_high(void) { DS1620_PORT->BSRR = RST_MASK; }
static inline void rst_low(void)  { DS1620_PORT->BSRR = RST_MASK << 16U; }

/* DQ: switch between output and input */
static inline void dq_output(void)
{
    DS1620_PORT->MODER &= ~(3U << (DS1620_DQ_PIN * 2U));
    DS1620_PORT->MODER |=  (1U << (DS1620_DQ_PIN * 2U));
}
static inline void dq_input(void)
{
    DS1620_PORT->MODER &= ~(3U << (DS1620_DQ_PIN * 2U)); /* input */
}
static inline void dq_high(void) { DS1620_PORT->BSRR = DQ_MASK; }
static inline void dq_low(void)  { DS1620_PORT->BSRR = DQ_MASK << 16U; }
static inline uint8_t dq_read(void)
{
    return (uint8_t)((DS1620_PORT->IDR >> DS1620_DQ_PIN) & 1U);
}

/* ── 3-wire low-level ───────────────────────────────────────────── */

/* Write 8-bit command, LSB first */
static void ds1620_write_byte(uint8_t byte)
{
    dq_output();
    for (uint8_t i = 0U; i < 8U; i++)
    {
        if (byte & 0x01U) dq_high(); else dq_low();
        delay_us(1U);
        clk_high();
        delay_us(1U);
        clk_low();
        delay_us(1U);
        byte >>= 1U;
    }
}

/*
 * Read n bits, LSB first.
 *
 * DS1620 timing: bit 0 is placed on DQ on the FALLING edge of the last
 * command-byte CLK (i.e. before this function is called, CLK is already
 * LOW and DQ already carries bit 0).  Each subsequent bit appears after
 * the next falling edge.  So: read current DQ, then clock to get the
 * next bit, repeat.
 */
static uint16_t ds1620_read_bits(uint8_t n)
{
    dq_input();
    delay_us(2U);   /* tDV: let DS1620 settle bit 0 on DQ */

    uint16_t val = 0U;
    for (uint8_t i = 0U; i < n; i++)
    {
        if (dq_read()) val |= (uint16_t)(1U << i);  /* sample before next edge */
        clk_high();
        delay_us(1U);
        clk_low();
        delay_us(2U);   /* DS1620 outputs next bit after falling edge */
    }
    return val;
}

/* ── Public API ─────────────────────────────────────────────────── */

void DS1620_Init(void)
{
    /* Enable GPIOB clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;

    /* CLK, RST — push-pull outputs, start LOW */
    DS1620_PORT->MODER &= ~((3U << (DS1620_CLK_PIN * 2U)) | (3U << (DS1620_RST_PIN * 2U)));
    DS1620_PORT->MODER |=   (1U << (DS1620_CLK_PIN * 2U)) | (1U << (DS1620_RST_PIN * 2U));
    DS1620_PORT->OTYPER  &= ~(CLK_MASK | RST_MASK);          /* push-pull */
    DS1620_PORT->OSPEEDR |=  ((3U << (DS1620_CLK_PIN * 2U)) | (3U << (DS1620_RST_PIN * 2U)));
    DS1620_PORT->BSRR     =  (CLK_MASK | RST_MASK) << 16U;   /* low       */

    /* DQ — open-drain output with pull-up */
    DS1620_PORT->MODER &= ~(3U << (DS1620_DQ_PIN * 2U));
    DS1620_PORT->MODER |=  (1U << (DS1620_DQ_PIN * 2U));     /* output    */
    DS1620_PORT->OTYPER |=  DQ_MASK;                          /* open-drain*/
    DS1620_PORT->PUPDR  &= ~(3U << (DS1620_DQ_PIN * 2U));
    DS1620_PORT->PUPDR  |=  (1U << (DS1620_DQ_PIN * 2U));    /* pull-up   */
    DS1620_PORT->BSRR    =  DQ_MASK;                          /* idle HIGH */

    /* Enable DWT cycle counter */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT       = 0U;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;

    /*
     * Write config: CPU=1 (CPU-driven mode), 1SHOT=0 (continuous)
     * Command 0xAC + 1 data byte
     */
    rst_high();
    delay_us(1U);
    ds1620_write_byte(0xACU);   /* Write Config */
    ds1620_write_byte(0x02U);   /* CPU=1, 1SHOT=0 */
    rst_low();
    delay_us(1U);
}

DS1620_Status DS1620_ReadTemp(float *temp_out)
{
    /* Issue Start Convert T */
    rst_high();
    delay_us(1U);
    ds1620_write_byte(0xEEU);   /* Start Convert T */
    rst_low();

    /* Wait for conversion (750 ms max per datasheet) */
    delay_ms(750U);

    /* Read Temperature — 9 bits, LSB first */
    rst_high();
    delay_us(1U);
    ds1620_write_byte(0xAAU);   /* Read Temperature */
    uint16_t raw = ds1620_read_bits(9U);
    rst_low();

    /*
     * 9-bit two's complement: bit8 = sign, bits[7:0] = magnitude.
     * Each LSB = 0.5°C.
     * Sign-extend to int16_t for negative temperatures.
     */
    int16_t temp_raw;
    if (raw & 0x100U)
    {
        /* Negative: sign-extend 9-bit value */
        temp_raw = (int16_t)(raw | 0xFF00U);
    }
    else
    {
        temp_raw = (int16_t)raw;
    }

    *temp_out = (float)temp_raw * 0.5f;
    return DS1620_OK;
}
