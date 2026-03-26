/*
 * epd213.h — Driver for WeAct Studio 2.13" e-Paper Module (SSD1680)
 *
 * Display:  GDEY0213Z98  122 × 250 px  B/W/RED (3-color)
 * Interface: 4-wire SPI, Mode 0 (CPOL=0 CPHA=0), MSB first
 *
 * Wiring (WeAct Black Pill STM32F411 → EPD module):
 *   VCC  → 3.3V
 *   GND  → GND
 *   DIN  → PA7   (SPI1_MOSI, AF5)
 *   CLK  → PA5   (SPI1_SCK,  AF5)
 *   CS   → PA4   (GPIO out, active LOW)
 *   DC   → PB0   (GPIO out, 0=Command 1=Data)
 *   RST  → PB1   (GPIO out, active LOW)
 *   BUSY → PB10  (GPIO in,  HIGH=busy)
 */

#ifndef EPD213_H
#define EPD213_H

#include <stdint.h>

/* ── Display geometry ─────────────────────────────────────────────── */
#define EPD_WIDTH           122U
#define EPD_HEIGHT          250U
#define EPD_BYTES_PER_ROW   ((EPD_WIDTH + 7U) / 8U)        /* = 16  */
#define EPD_BUFFER_SIZE     (EPD_BYTES_PER_ROW * EPD_HEIGHT) /* = 4000 */

/*
 * Image byte layout (row-major, MSB = leftmost pixel):
 *   bw_buf[row * EPD_BYTES_PER_ROW + col_byte]  — BW layer
 *   red_buf[row * EPD_BYTES_PER_ROW + col_byte] — Red layer
 *
 * BW layer  (0x24 RAM): 1 = white, 0 = black
 * Red layer (0x26 RAM): 1 = red,   0 = not red
 * Priority: red_buf bit=1 → pixel is RED regardless of bw_buf
 */

/* ── Pixel color constants ───────────────────────────────────────── */
#define EPD_COLOR_BLACK   0U
#define EPD_COLOR_WHITE   1U
#define EPD_COLOR_RED     2U

/* ── Pin definitions (GPIO driver encoding: port*16 + bit) ──────── */
#define EPD_CS_PIN      4U    /* PA4  */
#define EPD_DC_PIN      16U   /* PB0  */
#define EPD_RST_PIN     17U   /* PB1  */
#define EPD_BUSY_PIN    26U   /* PB10 */

/* ── Public API ──────────────────────────────────────────────────── */

/**
 * @brief  Initialize SPI1, control GPIOs and SSD1680 controller.
 *         Must be called once after SystemClock_Config().
 */
void EPD_Init(void);

/**
 * @brief  Fill the display with white and trigger a full refresh.
 */
void EPD_Clear(void);

/**
 * @brief  Push a full-screen image and trigger a full refresh.
 * @param  bw_buf   BW layer: EPD_BUFFER_SIZE bytes. 1=white, 0=black.
 * @param  red_buf  Red layer: EPD_BUFFER_SIZE bytes. 1=red, 0=not red.
 */
void EPD_Display(const uint8_t *bw_buf, const uint8_t *red_buf);

/**
 * @brief  Enter deep-sleep mode (draws ~0 µA).
 *         Call EPD_Init() again to wake the controller.
 */
void EPD_Sleep(void);

/**
 * @brief  Put a single character into the frame buffers.
 * @param  bw_buf   BW layer buffer (EPD_BUFFER_SIZE bytes).
 * @param  red_buf  Red layer buffer (EPD_BUFFER_SIZE bytes).
 * @param  x        Left edge of character.
 * @param  y        Top edge of character.
 * @param  c        ASCII character (32–127).
 * @param  color    EPD_COLOR_BLACK / EPD_COLOR_WHITE / EPD_COLOR_RED.
 */
void EPD_DrawChar(uint8_t *bw_buf, uint8_t *red_buf,
                  int16_t x, int16_t y, char c, uint8_t color);

/**
 * @brief  Draw a null-terminated ASCII string into the frame buffers.
 * @param  bw_buf   BW layer buffer.
 * @param  red_buf  Red layer buffer.
 * @param  x        Left edge of first character.
 * @param  y        Top edge.
 * @param  str      Null-terminated string.
 * @param  color    EPD_COLOR_BLACK / EPD_COLOR_WHITE / EPD_COLOR_RED.
 */
void EPD_DrawString(uint8_t *bw_buf, uint8_t *red_buf,
                    int16_t x, int16_t y, const char *str, uint8_t color);

/**
 * @brief  Draw a single character scaled up by `scale` times.
 * @param  scale  Pixel magnification factor (2=10x14, 3=15x21, 4=20x28).
 */
void EPD_DrawChar_Big(uint8_t *bw_buf, uint8_t *red_buf,
                      int16_t x, int16_t y, char c, uint8_t color, uint8_t scale);

/**
 * @brief  Draw a scaled string. Char width = (5+1)*scale pixels.
 */
void EPD_DrawString_Big(uint8_t *bw_buf, uint8_t *red_buf,
                        int16_t x, int16_t y, const char *str,
                        uint8_t color, uint8_t scale);

/*
 * Partial-update window for the temperature number area.
 * Rows 14..50 sit between the two separator lines (row 13 and row 51)
 * and contain only the big-font temperature digits (y=20, 28 px tall).
 * These rows have no red pixels, so the BW-only partial waveform is safe.
 */
#define EPD_TEMP_Y0   14U
#define EPD_TEMP_Y1   50U

/**
 * @brief  Fast partial refresh of a horizontal band (BW layer only).
 *
 * Sends bw_rows data for rows y0..y1 (full width) to the SSD1680 and
 * triggers the partial-update waveform (0xFF).  The red layer is
 * untouched — only the BW pixels in the specified rows are refreshed.
 *
 * @param  bw_rows  Pointer to the first byte of the BW buffer at row y0
 *                  (i.e. bw_buf + y0 * EPD_BYTES_PER_ROW).
 * @param  y0       First row of the update window (inclusive).
 * @param  y1       Last  row of the update window (inclusive).
 */
void EPD_PartialUpdate(const uint8_t *bw_rows, uint16_t y0, uint16_t y1);

#endif /* EPD213_H */
