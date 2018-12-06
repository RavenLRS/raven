#pragma once

#include <hal/gpio_base.h>

#define HAL_GPIO_PORT_A 0
#define HAL_GPIO_PORT_B 1
#define HAL_GPIO_PORT_C 2
#define HAL_GPIO_PORT_D 3
#define HAL_GPIO_PORT_E 4
#define HAL_GPIO_PORT_F 5
#define HAL_GPIO_PORT_G 6

#define HAL_GPIO_PIN(port, number) ((hal_gpio_t)((port << 5) | (number & 0x1f)))

#define HAL_PA(n) HAL_GPIO_PIN(HAL_GPIO_PORT_A, n)
#define HAL_PB(n) HAL_GPIO_PIN(HAL_GPIO_PORT_B, n)
#define HAL_PC(n) HAL_GPIO_PIN(HAL_GPIO_PORT_C, n)
#define HAL_PD(n) HAL_GPIO_PIN(HAL_GPIO_PORT_D, n)
#define HAL_PE(n) HAL_GPIO_PIN(HAL_GPIO_PORT_E, n)
#define HAL_PF(n) HAL_GPIO_PIN(HAL_GPIO_PORT_F, n)
#define HAL_PG(n) HAL_GPIO_PIN(HAL_GPIO_PORT_G, n)
