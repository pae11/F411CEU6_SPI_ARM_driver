/*
 * GPIO_STM32F4xx.h
 * CMSIS Driver GPIO для STM32F4xx
 */

#ifndef GPIO_STM32F4XX_H_
#define GPIO_STM32F4XX_H_

#include "Driver_GPIO.h"
#include "stm32f4xx.h"

/* Кодирование номера пина: порт * 16 + пин */
/* Пример: PA0 = 0, PA15 = 15, PB0 = 16, PC13 = 45 */

#define GPIO_PORT(pin)   ((pin) >> 4)   /* Номер порта (0=A, 1=B, 2=C ...) */
#define GPIO_PIN(pin)    ((pin) & 0x0F) /* Номер пина 0..15                */

extern ARM_DRIVER_GPIO Driver_GPIO_A;
extern ARM_DRIVER_GPIO Driver_GPIO_B;
extern ARM_DRIVER_GPIO Driver_GPIO_C;
extern ARM_DRIVER_GPIO Driver_GPIO_D;
extern ARM_DRIVER_GPIO Driver_GPIO_E;

#endif /* GPIO_STM32F4XX_H_ */
