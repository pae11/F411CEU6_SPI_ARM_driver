/*
 * ds1620.h — Dallas DS1620 Digital Thermometer driver
 *
 * Interface: 3-wire serial (CLK, DQ, RST)
 * Resolution: 9-bit, 0.5°C per LSB
 * Range: -55°C to +125°C
 * Conversion time: 750 ms max
 *
 * Pin mapping (all on GPIOB):
 *   PB3 — CLK  (output)
 *   PB4 — DQ   (bidirectional, open-drain + pull-up)
 *   PB5 — RST  (output, active HIGH to enable comms)
 */

#ifndef DS1620_H
#define DS1620_H

#include <stdint.h>

/* ── Return codes ───────────────────────────────────────────────── */
typedef enum {
    DS1620_OK      = 0,
    DS1620_NO_DATA = 1,   /* Conversion not ready or timeout */
} DS1620_Status;

/* ── Public API ─────────────────────────────────────────────────── */

/**
 * @brief  Enable GPIOB clock, configure CLK/DQ/RST pins, send
 *         CPU-mode config byte to DS1620 (continuous conversion,
 *         CPU mode — no thermostat outputs used).
 *         Must be called once after SystemClock_Config().
 */
void DS1620_Init(void);

/**
 * @brief  Start a temperature conversion, wait 750 ms, read result.
 *         Blocks for ~750 ms.
 * @param  temp_out  Pointer to float, result in °C (0.5°C resolution).
 * @return DS1620_OK or DS1620_NO_DATA.
 */
DS1620_Status DS1620_ReadTemp(float *temp_out);

#endif /* DS1620_H */
