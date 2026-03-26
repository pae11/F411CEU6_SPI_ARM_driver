/*
 * GPIO_STM32F4xx.c
 * Реализация CMSIS Driver GPIO для STM32F4xx
 *
 * Кодирование ARM_GPIO_Pin_t:
 *   Порт A: пины 0..15  (PA0=0  .. PA15=15)
 *   Порт B: пины 16..31 (PB0=16 .. PB15=31)
 *   Порт C: пины 32..47 (PC0=32 .. PC13=45)
 *   и т.д.
 */

#include "GPIO_STM32F4xx.h"

/* ------------------------------------------------------------------ */
/* Вспомогательные функции                                             */
/* ------------------------------------------------------------------ */

static GPIO_TypeDef *get_port(ARM_GPIO_Pin_t pin)
{
    static GPIO_TypeDef * const ports[] = {
        GPIOA, GPIOB, GPIOC, GPIOD, GPIOE
    };
    uint32_t port_idx = pin >> 4;
    if (port_idx >= (sizeof(ports) / sizeof(ports[0]))) return NULL;
    return ports[port_idx];
}

static void enable_port_clock(ARM_GPIO_Pin_t pin)
{
    uint32_t port_idx = pin >> 4;
    RCC->AHB1ENR |= (1UL << port_idx);
    (void)RCC->AHB1ENR; /* небольшая задержка после включения тактирования */
}

/* ------------------------------------------------------------------ */
/* Реализация функций драйвера                                         */
/* ------------------------------------------------------------------ */

static int32_t GPIO_Setup(ARM_GPIO_Pin_t pin, ARM_GPIO_SignalEvent_t cb_event)
{
    (void)cb_event; /* прерывания не используются в данной реализации */

    GPIO_TypeDef *port = get_port(pin);
    if (port == NULL) return ARM_DRIVER_ERROR_PARAMETER;

    enable_port_clock(pin);

    uint32_t pn = pin & 0x0FU;

    /* По умолчанию: вход, без подтяжки */
    port->MODER   &= ~(3UL << (pn * 2));   /* MODER: 00 = Input */
    port->PUPDR   &= ~(3UL << (pn * 2));   /* PUPDR: 00 = No pull */
    port->OTYPER  &= ~(1UL << pn);          /* OTYPER: 0 = Push-pull */
    port->OSPEEDR |=  (3UL << (pn * 2));   /* OSPEEDR: 11 = High speed */

    return ARM_DRIVER_OK;
}

static int32_t GPIO_SetDirection(ARM_GPIO_Pin_t pin, ARM_GPIO_DIRECTION direction)
{
    GPIO_TypeDef *port = get_port(pin);
    if (port == NULL) return ARM_DRIVER_ERROR_PARAMETER;

    uint32_t pn = pin & 0x0FU;

    port->MODER &= ~(3UL << (pn * 2));
    if (direction == ARM_GPIO_OUTPUT)
    {
        port->MODER |= (1UL << (pn * 2)); /* MODER: 01 = Output */
    }
    /* ARM_GPIO_INPUT: MODER = 00, уже установлено */

    return ARM_DRIVER_OK;
}

static int32_t GPIO_SetOutputMode(ARM_GPIO_Pin_t pin, ARM_GPIO_OUTPUT_MODE mode)
{
    GPIO_TypeDef *port = get_port(pin);
    if (port == NULL) return ARM_DRIVER_ERROR_PARAMETER;

    uint32_t pn = pin & 0x0FU;

    if (mode == ARM_GPIO_OPEN_DRAIN)
    {
        port->OTYPER |=  (1UL << pn); /* OTYPER: 1 = Open-drain */
    }
    else
    {
        port->OTYPER &= ~(1UL << pn); /* OTYPER: 0 = Push-pull  */
    }

    return ARM_DRIVER_OK;
}

static int32_t GPIO_SetPullResistor(ARM_GPIO_Pin_t pin, ARM_GPIO_PULL_RESISTOR resistor)
{
    GPIO_TypeDef *port = get_port(pin);
    if (port == NULL) return ARM_DRIVER_ERROR_PARAMETER;

    uint32_t pn = pin & 0x0FU;

    port->PUPDR &= ~(3UL << (pn * 2));

    switch (resistor)
    {
        case ARM_GPIO_PULL_UP:
            port->PUPDR |= (1UL << (pn * 2)); /* PUPDR: 01 = Pull-up */
            break;
        case ARM_GPIO_PULL_DOWN:
            port->PUPDR |= (2UL << (pn * 2)); /* PUPDR: 10 = Pull-down */
            break;
        default: /* ARM_GPIO_PULL_NONE */
            break;
    }

    return ARM_DRIVER_OK;
}

static int32_t GPIO_SetEventTrigger(ARM_GPIO_Pin_t pin, ARM_GPIO_EVENT_TRIGGER trigger)
{
    (void)pin;
    (void)trigger;
    /* Прерывания EXTI не реализованы в данном минимальном драйвере */
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static void GPIO_SetOutput(ARM_GPIO_Pin_t pin, uint32_t val)
{
    GPIO_TypeDef *port = get_port(pin);
    if (port == NULL) return;

    uint32_t pn = pin & 0x0FU;

    if (val)
    {
        port->BSRR = (1UL << pn);          /* Установить пин */
    }
    else
    {
        port->BSRR = (1UL << (pn + 16));   /* Сбросить пин */
    }
}

static uint32_t GPIO_GetInput(ARM_GPIO_Pin_t pin)
{
    GPIO_TypeDef *port = get_port(pin);
    if (port == NULL) return 0U;

    uint32_t pn = pin & 0x0FU;
    return (port->IDR >> pn) & 1U;
}

/* ------------------------------------------------------------------ */
/* Экземпляры драйверов для каждого порта                             */
/* ------------------------------------------------------------------ */

ARM_DRIVER_GPIO Driver_GPIO_A = {
    GPIO_Setup,
    GPIO_SetDirection,
    GPIO_SetOutputMode,
    GPIO_SetPullResistor,
    GPIO_SetEventTrigger,
    GPIO_SetOutput,
    GPIO_GetInput
};

ARM_DRIVER_GPIO Driver_GPIO_B = {
    GPIO_Setup,
    GPIO_SetDirection,
    GPIO_SetOutputMode,
    GPIO_SetPullResistor,
    GPIO_SetEventTrigger,
    GPIO_SetOutput,
    GPIO_GetInput
};

ARM_DRIVER_GPIO Driver_GPIO_C = {
    GPIO_Setup,
    GPIO_SetDirection,
    GPIO_SetOutputMode,
    GPIO_SetPullResistor,
    GPIO_SetEventTrigger,
    GPIO_SetOutput,
    GPIO_GetInput
};

ARM_DRIVER_GPIO Driver_GPIO_D = {
    GPIO_Setup,
    GPIO_SetDirection,
    GPIO_SetOutputMode,
    GPIO_SetPullResistor,
    GPIO_SetEventTrigger,
    GPIO_SetOutput,
    GPIO_GetInput
};

ARM_DRIVER_GPIO Driver_GPIO_E = {
    GPIO_Setup,
    GPIO_SetDirection,
    GPIO_SetOutputMode,
    GPIO_SetPullResistor,
    GPIO_SetEventTrigger,
    GPIO_SetOutput,
    GPIO_GetInput
};
