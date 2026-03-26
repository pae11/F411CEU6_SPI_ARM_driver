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
 *
 * Commands used:
 *   0x01 — Write TH  (9-bit, written once in Init to EEPROM)
 *   0x02 — Write TL  (9-bit, written once in Init to EEPROM)
 *   0xAC — Write Config
 *   0xEE — Start Convert T
 *   0xAA — Read Temperature
 *
 * Thermostat outputs (CPU mode — still active, can be used as HW safety):
 *   THIGH (pin 5) = HIGH when T >= TH  → signals over-temperature
 *   TLOW  (pin 6) = HIGH when T <= TL  → signals under-temperature
 *   TOUT  (pin 7) = HIGH when TL < T < TH
 *
 * DS1620_TH_CUTOFF / DS1620_TL_CUTOFF are written to EEPROM in DS1620_Init()
 * so that when THIGH is later wired as hardware cutoff it is already configured.
 */

#ifndef DS1620_H
#define DS1620_H

#include <stdint.h>

/*
 * Hardware safety thresholds written to DS1620 TH/TL EEPROM registers.
 * Keep TH a few degrees above the software setpoint so THIGH can serve
 * as a failsafe cutoff when wired to the MOSFET gate later.
 * Values must be multiples of 0.5°C (DS1620 resolution).
 */
#define DS1620_TH_CUTOFF   30.0f    /* °C — TH register: THIGH goes HIGH at T >= this */
#define DS1620_TL_CUTOFF    0.0f    /* °C — TL register: TLOW  goes HIGH at T <= this */

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
