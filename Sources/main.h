#ifndef MAIN_H
#define MAIN_H

#include "stm32f4xx.h"
#include "Driver_GPIO.h"  // Подключаем интерфейс GPIO драйвера
#include "EPD/epd213.h"   // E-Paper дисплей 2.13"

// Определяем частоту тактирования системы
#define SYSTEM_CORE_CLOCK    100000000U  // 100 MHz

// Параметры мигания светодиодом
#define LED_BLINK_INTERVAL   50         // Интервал мигания в миллисекундах

// Пользовательские функции
void SystemClock_Config(void);
void LED_Initialize(void);
void delay_ms(uint32_t ms);
void EPD_Demo(void);

#endif /* MAIN_H */
