#pragma once

#include <hal/gpio.h>

typedef enum
{
    GPIO_TAG_INPUT_TX,
    GPIO_TAG_INPUT_RX,
    GPIO_TAG_INPUT_BIDIR,
    GPIO_TAG_OUTPUT_TX,
    GPIO_TAG_OUTPUT_RX,
    GPIO_TAG_OUTPUT_BIDIR,
} gpio_tag_e;

hal_gpio_t gpio_get_configurable_at(unsigned idx);
hal_gpio_t gpio_get_by_tag(gpio_tag_e tag);
// This function uses a shared static buffer and is
// not thread safe.
char *gpio_toa(hal_gpio_t gpio);
