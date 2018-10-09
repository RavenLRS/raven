#pragma once

#include <stddef.h>
#include <stdint.h>

#include <hal/err.h>

typedef uint8_t hal_gpio_t;
typedef uint64_t hal_gpio_mask_t;

#define HAL_GPIO_NONE ((hal_gpio_t)0xFF)

#define HAL_GPIO_MAX 63
#define HAL_GPIO_FULL_MASK 0xFFFFFFFFFFFFFFFFll
#define HAL_GPIO_M(x) ((uint64_t)1 << x)
#define HAL_GPIO_MASK_COUNT(v) (__builtin_popcountll(v))

#define HAL_GPIO_USER_MAX 16
#define HAL_GPIO_USER_COUNT HAL_GPIO_MASK_COUNT(HAL_GPIO_USER_MASK)
#define HAL_GPIO_IS_USER(x) (HAL_GPIO_USER_MASK & HAL_GPIO_M(x))
#define HAL_GPIO_USER_GET_IDX(n) (__builtin_popcountll(HAL_GPIO_USER_MASK & (~0ull >> (HAL_GPIO_MAX - n + 1))))

#define HAL_GPIO_HIGH 1
#define HAL_GPIO_LOW 0

#define HAL_GPIO_NAME_LENGTH 4 // includes '\0' terminator - esp32: %02d, stm32: %c%02d - TODO: Use 3 chars on esp32

typedef enum
{
    HAL_GPIO_DIR_INPUT,
    HAL_GPIO_DIR_OUTPUT,
    HAL_GPIO_DIR_OUTPUT_OD,
    HAL_GPIO_DIR_BIDIR,
} hal_gpio_dir_t;

typedef enum
{
    HAL_GPIO_PULL_NONE,
    HAL_GPIO_PULL_UP,
    HAL_GPIO_PULL_DOWN,
    HAL_GPIO_PULL_BOTH,
} hal_gpio_pull_t;

typedef enum
{
    HAL_GPIO_INTR_POSEDGE,
    HAL_GPIO_INTR_NEGEDGE,
    HAL_GPIO_INTR_ANYEDGE,
    HAL_GPIO_INTR_LOW_LEVEL,
    HAL_GPIO_INTR_HIGH_LEVEL,
} hal_gpio_intr_t;

hal_err_t hal_gpio_setup(hal_gpio_t gpio, hal_gpio_dir_t dir, hal_gpio_pull_t pull);
hal_err_t hal_gpio_set_level(hal_gpio_t gpio, uint32_t level);
hal_err_t hal_gpio_get_level(hal_gpio_t gpio);

typedef void (*hal_gpio_isr_t)(void *);
int hal_gpio_set_isr(hal_gpio_t gpio, hal_gpio_intr_t intr, hal_gpio_isr_t isr, const void *data);

char *hal_gpio_toa(hal_gpio_t gpio, char *dst, size_t size);
