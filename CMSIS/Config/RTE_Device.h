#ifndef __RTE_DEVICE_H
#define __RTE_DEVICE_H

/* CMSIS Driver для GPIO */
// Включаем поддержку всех необходимых портов GPIO
#define RTE_GPIO_A                 1    // Включить GPIOA
#define RTE_GPIO_B                 1    // Включить GPIOB
#define RTE_GPIO_C                 1    // Включить GPIOC - нужен для LED на PC13
#define RTE_GPIO_D                 0    // Отключить GPIOD
#define RTE_GPIO_E                 0    // Отключить GPIOE
#define RTE_GPIO_H                 0    // Отключить GPIOH

/* CMSIS Driver для SPI */
// Настройки для SPI1 (если понадобится в будущем)
#define RTE_SPI1                  0    // Отключен, включите если нужен
#define RTE_SPI1_DMA_TX_EN        0    // Отключен DMA для TX
#define RTE_SPI1_DMA_RX_EN        0    // Отключен DMA для RX

/* CMSIS Driver для USART */
// Настройки для USART1 (если понадобится в будущем)
#define RTE_USART1                0    // Отключен, включите если нужен
#define RTE_USART1_DMA_TX_EN      0    // Отключен DMA для TX
#define RTE_USART1_DMA_RX_EN      0    // Отключен DMA для RX

/* CMSIS Driver для I2C */
// Настройки для I2C1 (если понадобится в будущем)
#define RTE_I2C1                  0    // Отключен, включите если нужен
#define RTE_I2C1_DMA_TX_EN        0    // Отключен DMA для TX
#define RTE_I2C1_DMA_RX_EN        0    // Отключен DMA для RX

#endif /* __RTE_DEVICE_H */
