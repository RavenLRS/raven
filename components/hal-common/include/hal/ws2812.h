#pragma once

#include <stddef.h>
#include <stdint.h>

#include <hal/err.h>
#include <hal/gpio.h>

#define HAL_WS2812_COLOR_LEVEL_MAX 255
#define HAL_WS2812_COLOR_STEP_MIN INT8_MIN
#define HAL_WS2812_COLOR_STEP_MAX INT8_MAX

typedef struct hal_ws2812_color_s
{
    // Declared in the same order as they're sent out,
    // so we can avoid a copy to reorder the data in memory.
    uint8_t g;
    uint8_t r;
    uint8_t b;
} __attribute__((packed)) hal_ws2812_color_t;

typedef struct hal_ws2812_color_step_s
{
    int8_t r;
    int8_t g;
    int8_t b;
} __attribute__((packed)) hal_ws2812_color_step_t;

typedef struct hal_ws2812_state_s
{
    hal_ws2812_color_t color;        // Current color
    hal_ws2812_color_t target_color; // Color we want to transition to
} hal_ws2812_state_t;

#define HAL_WS2812_RGB(red, green, blue) \
    {                                    \
        .g = green, .r = red, .b = blue  \
    }

#define HAL_WS2812_RED HAL_WS2812_RGB(HAL_WS2812_COLOR_LEVEL_MAX, 0, 0)
#define HAL_WS2812_GREEN HAL_WS2812_RGB(0, HAL_WS2812_COLOR_LEVEL_MAX, 0)
#define HAL_WS2812_BLUE HAL_WS2812_RGB(0, 0, HAL_WS2812_COLOR_LEVEL_MAX)
#define HAL_WS2812_WHITE HAL_WS2812_RGB(HAL_WS2812_COLOR_LEVEL_MAX, HAL_WS2812_COLOR_LEVEL_MAX, HAL_WS2812_COLOR_LEVEL_MAX)
#define HAL_WS2812_OFF HAL_WS2812_RGB(0, 0, 0)

hal_err_t hal_ws2812_open(hal_gpio_t gpio);
hal_err_t hal_ws2812_close(hal_gpio_t gpio);
hal_err_t hal_ws2812_set_colors(hal_gpio_t gpio, const hal_ws2812_color_t *colors, size_t count);