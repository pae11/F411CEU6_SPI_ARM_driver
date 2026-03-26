#ifndef MAIN_H
#define MAIN_H

#include "stm32f4xx.h"
#include "Driver_GPIO.h"  // Подключаем интерфейс GPIO драйвера
#include "EPD/epd213.h"   // E-Paper дисплей 2.13"
#include "DS1620/ds1620.h" // Dallas DS1620 3-wire термометр

// Определяем частоту тактирования системы
#define SYSTEM_CORE_CLOCK    100000000U  // 100 MHz

// Параметры мигания светодиодом
#define LED_BLINK_INTERVAL   500        // Интервал мигания в миллисекундах

/*
 * Термостат
 *   RELAY_PIN     — PA6, выход на транзистор/МОСФЕТ (active HIGH = нагрев включён)
 *   TEMP_SETPOINT — уставка в °C (DS1620 шаг 0.5°C, ставьте кратно 0.5)
 *   TEMP_HYST     — гистерезис в °C: включается при < SET-HYST, выключается при > SET+HYST
 */
#define RELAY_PIN        6U      /* PA6 */
#define TEMP_SETPOINT    22.0f  /* °C  */
#define TEMP_HYST         1.0f  /* °C  */

// Пользовательские функции
void SystemClock_Config(void);
void LED_Initialize(void);
void Relay_Initialize(void);
void delay_ms(uint32_t ms);
void Temp_Demo(void);

#endif /* MAIN_H */
