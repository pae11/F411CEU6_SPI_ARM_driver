/*
 * epd213.c — SSD1680 driver for WeAct Studio 2.13" e-Paper Module
 *
 * SPI1 hardware is used at 12.5 MHz (PCLK2/8 = 100MHz/8).
 * Bulk TX transfers (frame buffer) use DMA2 Stream3 Channel3.
 * Single-byte command/data transfers use polling.
 * Control pins (CS, DC, RST, BUSY) are managed via the CMSIS GPIO driver.
 */

#include "EPD/epd213.h"
#include "EPD/font5x7.h"
#include "main.h"           /* delay_ms(), stm32f4xx.h (SPI1, RCC, GPIOA/B) */
#include "Driver_GPIO.h"

/* ── CMSIS GPIO driver instances (defined in GPIO_STM32F4xx.c) ────── */
extern ARM_DRIVER_GPIO Driver_GPIO_A;
extern ARM_DRIVER_GPIO Driver_GPIO_B;

static ARM_DRIVER_GPIO *epd_gpioa;
static ARM_DRIVER_GPIO *epd_gpiob;

/* ================================================================== */
/* DMA2 Stream3 — SPI1_TX (Channel 3)                                */
/* ================================================================== */

#define EPD_DMA_STREAM      DMA2_Stream3
#define EPD_DMA_CHANNEL     (3UL << DMA_SxCR_CHSEL_Pos)  /* CH3 */
#define EPD_DMA_TCIF        DMA_LISR_TCIF3                /* stream 3 in LISR */
#define EPD_DMA_CLEAR_TC    DMA_LIFCR_CTCIF3
#define EPD_DMA_CLEAR_ALL   (DMA_LIFCR_CTCIF3  | DMA_LIFCR_CHTIF3  | \
                             DMA_LIFCR_CTEIF3  | DMA_LIFCR_CDMEIF3 | \
                             DMA_LIFCR_CFEIF3)

/* ================================================================== */
/* SPI1 hardware + DMA                                                */
/* ================================================================== */

static void DMA2_SPI1TX_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    (void)RCC->AHB1ENR;

    /* Disable stream before configuration */
    EPD_DMA_STREAM->CR &= ~DMA_SxCR_EN;
    while (EPD_DMA_STREAM->CR & DMA_SxCR_EN);

    /* Peripheral address: fixed to SPI1 data register (8-bit access) */
    EPD_DMA_STREAM->PAR = (uint32_t)(&SPI1->DR);

    /* Direct mode (no FIFO threshold), FIFO error interrupt off */
    EPD_DMA_STREAM->FCR = 0U;

    /*
     * CR fixed fields (variable MINC / M0AR set per-transfer):
     *   CH3       – select SPI1_TX request
     *   DIR=01    – memory → peripheral
     *   PL=10     – high priority
     *   MSIZE=00  – 8-bit memory data
     *   PSIZE=00  – 8-bit peripheral data
     *   CIRC=0    – no circular
     *   PINC=0    – peripheral no increment
     */
    EPD_DMA_STREAM->CR = EPD_DMA_CHANNEL
                       | DMA_SxCR_DIR_0          /* mem → periph */
                       | DMA_SxCR_PL_1;          /* high priority */
}

static void SPI1_Init(void)
{
    /* Enable peripheral clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    (void)RCC->APB2ENR;

    /* PA5 = SCK, PA7 = MOSI  →  AF mode (MODER = 10) */
    GPIOA->MODER &= ~((3UL << (5U * 2U)) | (3UL << (7U * 2U)));
    GPIOA->MODER |=  ((2UL << (5U * 2U)) | (2UL << (7U * 2U)));
    GPIOA->OSPEEDR |= ((3UL << (5U * 2U)) | (3UL << (7U * 2U)));
    GPIOA->PUPDR   &= ~((3UL << (5U * 2U)) | (3UL << (7U * 2U)));
    GPIOA->AFR[0]  &= ~((0xFUL << (5U * 4U)) | (0xFUL << (7U * 4U)));
    GPIOA->AFR[0]  |=  ((5UL   << (5U * 4U)) | (5UL   << (7U * 4U)));

    /*
     * SPI1 CR1: master, Mode 0, 8-bit, MSB first, SW-NSS, fPCLK/8 = 12.5 MHz
     * SPE is set last after DMA is ready.
     */
    SPI1->CR1 = SPI_CR1_MSTR
              | SPI_CR1_SSM
              | SPI_CR1_SSI
              | (2UL << SPI_CR1_BR_Pos);   /* /8 = 12.5 MHz */

    /* Enable SPI TX DMA request */
    SPI1->CR2 |= SPI_CR2_TXDMAEN;

    SPI1->CR1 |= SPI_CR1_SPE;

    /* Init DMA2 Stream3 for SPI1_TX */
    DMA2_SPI1TX_Init();
}

/* Send one byte over SPI1 (polling — used for single-byte commands/data) */
static void SPI1_SendByte(uint8_t byte)
{
    /* Temporarily disable TX DMA request so this byte uses polling path */
    SPI1->CR2 &= ~SPI_CR2_TXDMAEN;
    while (!(SPI1->SR & SPI_SR_TXE));
    *((__IO uint8_t *)&SPI1->DR) = byte;
    while (SPI1->SR & SPI_SR_BSY);
    /* Clear overrun from the RX side */
    (void)SPI1->DR;
    (void)SPI1->SR;
    SPI1->CR2 |= SPI_CR2_TXDMAEN;
}

/*
 * SPI1_DMA_Send — bulk TX via DMA2 Stream3
 *   buf  != NULL : send from buffer (MINC=1)
 *   buf  == NULL : send fill byte len times (MINC=0)
 */
static void SPI1_DMA_Send(const uint8_t *buf, uint8_t fill, uint32_t len)
{
    static uint8_t s_fill;

    /* Ensure stream is off */
    EPD_DMA_STREAM->CR &= ~DMA_SxCR_EN;
    while (EPD_DMA_STREAM->CR & DMA_SxCR_EN);

    /* Clear all interrupt flags for stream 3 */
    DMA2->LIFCR = EPD_DMA_CLEAR_ALL;

    EPD_DMA_STREAM->NDTR = len;

    if (buf != NULL)
    {
        EPD_DMA_STREAM->M0AR = (uint32_t)buf;
        EPD_DMA_STREAM->CR  |=  DMA_SxCR_MINC;   /* increment through buffer */
    }
    else
    {
        s_fill = fill;
        EPD_DMA_STREAM->M0AR = (uint32_t)&s_fill;
        EPD_DMA_STREAM->CR  &= ~DMA_SxCR_MINC;   /* repeat same byte */
    }

    /* Enable stream — SPI TX DMA request already armed in CR2 */
    EPD_DMA_STREAM->CR |= DMA_SxCR_EN;

    /* Wait for DMA transfer-complete flag */
    while (!(DMA2->LISR & EPD_DMA_TCIF));

    /* Wait until SPI shift register drains */
    while (SPI1->SR & SPI_SR_BSY);

    /* Disable stream and clear flags */
    EPD_DMA_STREAM->CR &= ~DMA_SxCR_EN;
    DMA2->LIFCR = EPD_DMA_CLEAR_ALL;

    /* Clear SPI RX overrun (inevitable in TX-only DMA) */
    (void)SPI1->DR;
    (void)SPI1->SR;
}

/* ================================================================== */
/* Control GPIO init                                                   */
/* ================================================================== */

static void EPD_GPIO_Init(void)
{
    epd_gpioa = &Driver_GPIO_A;
    epd_gpiob = &Driver_GPIO_B;

    /* CS: PA4 → output push-pull, start HIGH (deselected) */
    epd_gpioa->Setup(EPD_CS_PIN, NULL);
    epd_gpioa->SetDirection(EPD_CS_PIN, ARM_GPIO_OUTPUT);
    epd_gpioa->SetOutputMode(EPD_CS_PIN, ARM_GPIO_PUSH_PULL);
    epd_gpioa->SetPullResistor(EPD_CS_PIN, ARM_GPIO_PULL_NONE);
    epd_gpioa->SetOutput(EPD_CS_PIN, 1U);

    /* DC: PB0 → output push-pull */
    epd_gpiob->Setup(EPD_DC_PIN, NULL);
    epd_gpiob->SetDirection(EPD_DC_PIN, ARM_GPIO_OUTPUT);
    epd_gpiob->SetOutputMode(EPD_DC_PIN, ARM_GPIO_PUSH_PULL);
    epd_gpiob->SetPullResistor(EPD_DC_PIN, ARM_GPIO_PULL_NONE);

    /* RST: PB1 → output push-pull, start HIGH */
    epd_gpiob->Setup(EPD_RST_PIN, NULL);
    epd_gpiob->SetDirection(EPD_RST_PIN, ARM_GPIO_OUTPUT);
    epd_gpiob->SetOutputMode(EPD_RST_PIN, ARM_GPIO_PUSH_PULL);
    epd_gpiob->SetPullResistor(EPD_RST_PIN, ARM_GPIO_PULL_NONE);
    epd_gpiob->SetOutput(EPD_RST_PIN, 1U);

    /* BUSY: PB10 → input with pull-down */
    epd_gpiob->Setup(EPD_BUSY_PIN, NULL);
    epd_gpiob->SetDirection(EPD_BUSY_PIN, ARM_GPIO_INPUT);
    epd_gpiob->SetPullResistor(EPD_BUSY_PIN, ARM_GPIO_PULL_DOWN);
}

/* ================================================================== */
/* SSD1680 low-level helpers                                           */
/* ================================================================== */

static void EPD_SendCmd(uint8_t cmd)
{
    epd_gpiob->SetOutput(EPD_DC_PIN, 0U);  /* DC = 0 : command */
    epd_gpioa->SetOutput(EPD_CS_PIN, 0U);  /* CS low */
    SPI1_SendByte(cmd);
    epd_gpioa->SetOutput(EPD_CS_PIN, 1U);  /* CS high */
}

static void EPD_SendData(uint8_t data)
{
    epd_gpiob->SetOutput(EPD_DC_PIN, 1U);  /* DC = 1 : data */
    epd_gpioa->SetOutput(EPD_CS_PIN, 0U);
    SPI1_SendByte(data);
    epd_gpioa->SetOutput(EPD_CS_PIN, 1U);
}

/* Wait until BUSY pin goes low (controller ready) */
static void EPD_WaitBusy(void)
{
    delay_ms(10U);
    while (epd_gpiob->GetInput(EPD_BUSY_PIN))
    {
        delay_ms(10U);
    }
}

/* Hardware reset pulse */
static void EPD_HardReset(void)
{
    epd_gpiob->SetOutput(EPD_RST_PIN, 1U);
    delay_ms(20U);
    epd_gpiob->SetOutput(EPD_RST_PIN, 0U);
    delay_ms(5U);
    epd_gpiob->SetOutput(EPD_RST_PIN, 1U);
    delay_ms(20U);
}

/* Reset RAM address counters and window to full screen */
static void EPD_SetRamWindow(void)
{
    /* Data Entry Mode: X inc, Y inc (resend every time as GxEPD2 does) */
    EPD_SendCmd(0x11U);
    EPD_SendData(0x03U);

    /* RAM-X window: bytes 0 to 15 */
    EPD_SendCmd(0x44U);
    EPD_SendData(0x00U);
    EPD_SendData((uint8_t)(EPD_BYTES_PER_ROW - 1U));      /* 0x0F */

    /* RAM-Y window: rows 0 to 249 */
    EPD_SendCmd(0x45U);
    EPD_SendData(0x00U);
    EPD_SendData(0x00U);
    EPD_SendData((uint8_t)((EPD_HEIGHT - 1U) & 0xFFU));   /* 0xF9 */
    EPD_SendData((uint8_t)((EPD_HEIGHT - 1U) >> 8U));     /* 0x00 */

    /* Set RAM-X address counter = 0 */
    EPD_SendCmd(0x4EU);
    EPD_SendData(0x00U);

    /* Set RAM-Y address counter = 0 */
    EPD_SendCmd(0x4FU);
    EPD_SendData(0x00U);
    EPD_SendData(0x00U);
}

/* ================================================================== */
/* Public API                                                          */
/* ================================================================== */

void EPD_Init(void)
{
    EPD_GPIO_Init();
    SPI1_Init();

    /* Hardware reset */
    EPD_HardReset();

    /* Software reset — no WaitBusy here, just 10 ms delay (SSD1680 spec) */
    EPD_SendCmd(0x12U);
    delay_ms(10U);

    /* Driver Output Control: MUX = HEIGHT-1 = 249 = 0xF9 */
    EPD_SendCmd(0x01U);
    EPD_SendData(0xF9U);
    EPD_SendData(0x00U);
    EPD_SendData(0x00U);

    /* Border Waveform Control */
    EPD_SendCmd(0x3CU);
    EPD_SendData(0x05U);

    /* Display Update Control (required for SSD1680) */
    EPD_SendCmd(0x21U);
    EPD_SendData(0x00U);
    EPD_SendData(0x80U);

    /* Use built-in temperature sensor */
    EPD_SendCmd(0x18U);
    EPD_SendData(0x80U);

    /* Set RAM window + cursor */
    EPD_SetRamWindow();

    EPD_WaitBusy();
}

void EPD_Clear(void)
{
    EPD_SetRamWindow();

    /* Write BW RAM (0x24): all white */
    EPD_SendCmd(0x24U);
    epd_gpiob->SetOutput(EPD_DC_PIN, 1U);
    epd_gpioa->SetOutput(EPD_CS_PIN, 0U);
    SPI1_DMA_Send(NULL, 0xFFU, EPD_BUFFER_SIZE);
    epd_gpioa->SetOutput(EPD_CS_PIN, 1U);

    /* Write previous BW RAM (0x26): also white for clean full-refresh */
    EPD_SetRamWindow();
    EPD_SendCmd(0x26U);
    epd_gpiob->SetOutput(EPD_DC_PIN, 1U);
    epd_gpioa->SetOutput(EPD_CS_PIN, 0U);
    SPI1_DMA_Send(NULL, 0xFFU, EPD_BUFFER_SIZE);
    epd_gpioa->SetOutput(EPD_CS_PIN, 1U);

    /* Full update sequence (0xF7 = power-on + full refresh + power-off) */
    EPD_SendCmd(0x22U);
    EPD_SendData(0xF7U);
    EPD_SendCmd(0x20U);
    EPD_WaitBusy();
}

void EPD_Display(const uint8_t *bw_buf, const uint8_t *red_buf)
{
    EPD_SetRamWindow();

    /* Write current BW RAM (0x24) */
    EPD_SendCmd(0x24U);
    epd_gpiob->SetOutput(EPD_DC_PIN, 1U);
    epd_gpioa->SetOutput(EPD_CS_PIN, 0U);
    SPI1_DMA_Send(bw_buf, 0x00U, EPD_BUFFER_SIZE);
    epd_gpioa->SetOutput(EPD_CS_PIN, 1U);

    /* Write Red RAM (0x26): 1 = red pixel */
    EPD_SetRamWindow();
    EPD_SendCmd(0x26U);
    epd_gpiob->SetOutput(EPD_DC_PIN, 1U);
    epd_gpioa->SetOutput(EPD_CS_PIN, 0U);
    SPI1_DMA_Send(red_buf, 0x00U, EPD_BUFFER_SIZE);
    epd_gpioa->SetOutput(EPD_CS_PIN, 1U);

    /* Full update (power-on + refresh + power-off) */
    EPD_SendCmd(0x22U);
    EPD_SendData(0xF7U);
    EPD_SendCmd(0x20U);
    EPD_WaitBusy();
}

void EPD_Sleep(void)
{
    EPD_SendCmd(0x10U); /* Deep Sleep mode */
    EPD_SendData(0x01U);
}

/* ================================================================== */
/* Text rendering                                                      */
/* ================================================================== */

void EPD_DrawChar(uint8_t *bw_buf, uint8_t *red_buf,
                  int16_t x, int16_t y, char c, uint8_t color)
{
    if ((uint8_t)c < 32U || (uint8_t)c > 127U) c = '?';
    const uint8_t *glyph = font5x7[(uint8_t)c - 32U];

    for (int16_t cx = 0; cx < 5; cx++)
    {
        uint8_t col = glyph[cx];
        for (int16_t cy = 0; cy < 7; cy++)
        {
            int16_t px = x + cx;
            int16_t py = y + cy;
            if (px < 0 || px >= (int16_t)EPD_WIDTH)  continue;
            if (py < 0 || py >= (int16_t)EPD_HEIGHT) continue;

            uint32_t idx      = (uint32_t)py * EPD_BYTES_PER_ROW + (uint32_t)px / 8U;
            uint8_t  bit_mask = 0x80U >> ((uint32_t)px % 8U);

            if (col & (1U << (uint8_t)cy))
            {
                /* foreground pixel */
                if (color == EPD_COLOR_RED)
                {
                    bw_buf[idx]  |=  bit_mask;  /* BW=white (not black) */
                    red_buf[idx] |=  bit_mask;  /* Red=1 → red pixel  */
                }
                else if (color == EPD_COLOR_BLACK)
                {
                    bw_buf[idx]  &= ~bit_mask;  /* BW=black */
                    red_buf[idx] &= ~bit_mask;  /* Red=0 */
                }
                else /* EPD_COLOR_WHITE */
                {
                    bw_buf[idx]  |=  bit_mask;  /* BW=white */
                    red_buf[idx] &= ~bit_mask;  /* Red=0 */
                }
            }
            else
            {
                /* background: always white, no red */
                bw_buf[idx]  |=  bit_mask;
                red_buf[idx] &= ~bit_mask;
            }
        }
    }
}

void EPD_DrawString(uint8_t *bw_buf, uint8_t *red_buf,
                    int16_t x, int16_t y, const char *str, uint8_t color)
{
    while (*str != '\0')
    {
        EPD_DrawChar(bw_buf, red_buf, x, y, *str, color);
        str++;
        x += (int16_t)FONT_CHAR_W;
        if (x + (int16_t)FONT_CHAR_W > (int16_t)EPD_WIDTH) break;
    }
}

/* ── Scaled text rendering ────────────────────────────────────────── */

void EPD_DrawChar_Big(uint8_t *bw_buf, uint8_t *red_buf,
                      int16_t x, int16_t y, char c, uint8_t color, uint8_t scale)
{
    if ((uint8_t)c < 32U || (uint8_t)c > 127U) c = '?';
    const uint8_t *glyph = font5x7[(uint8_t)c - 32U];

    for (int16_t cx = 0; cx < 5; cx++)
    {
        uint8_t col = glyph[cx];
        for (int16_t cy = 0; cy < 7; cy++)
        {
            uint8_t fg = (col >> (uint8_t)cy) & 1U;
            for (int16_t sx = 0; sx < (int16_t)scale; sx++)
            {
                for (int16_t sy = 0; sy < (int16_t)scale; sy++)
                {
                    int16_t px = x + cx * (int16_t)scale + sx;
                    int16_t py = y + cy * (int16_t)scale + sy;
                    if (px < 0 || px >= (int16_t)EPD_WIDTH)  continue;
                    if (py < 0 || py >= (int16_t)EPD_HEIGHT) continue;

                    uint32_t idx      = (uint32_t)py * EPD_BYTES_PER_ROW + (uint32_t)px / 8U;
                    uint8_t  bit_mask = 0x80U >> ((uint32_t)px % 8U);

                    if (fg)
                    {
                        if (color == EPD_COLOR_RED)
                        {
                            bw_buf[idx]  |=  bit_mask;
                            red_buf[idx] |=  bit_mask;
                        }
                        else if (color == EPD_COLOR_BLACK)
                        {
                            bw_buf[idx]  &= ~bit_mask;
                            red_buf[idx] &= ~bit_mask;
                        }
                        else
                        {
                            bw_buf[idx]  |=  bit_mask;
                            red_buf[idx] &= ~bit_mask;
                        }
                    }
                    else
                    {
                        bw_buf[idx]  |=  bit_mask;
                        red_buf[idx] &= ~bit_mask;
                    }
                }
            }
        }
    }
}

void EPD_DrawString_Big(uint8_t *bw_buf, uint8_t *red_buf,
                        int16_t x, int16_t y, const char *str,
                        uint8_t color, uint8_t scale)
{
    int16_t step = (int16_t)((FONT_CHAR_W) * scale);
    while (*str != '\0')
    {
        EPD_DrawChar_Big(bw_buf, red_buf, x, y, *str, color, scale);
        str++;
        x += step;
        if (x + step > (int16_t)EPD_WIDTH) break;
    }
}


