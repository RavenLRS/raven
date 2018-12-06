#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <hal/gpio.h>

#include "target.h"

typedef struct button_state_s
{
    bool is_down;
    bool long_press_sent;
    bool really_long_press_sent;
    unsigned long down_since;
    bool ignore; // Used when button is pressed on startup
} button_state_t;

typedef struct button_s
{
    hal_gpio_t gpio;
#if defined(USE_TOUCH_BUTTON)
    bool is_touch;
#endif
    void *user_data;
    void (*press_callback)(void *data);
    void (*long_press_callback)(void *data);
    void (*really_long_press_callback)(void *data);
    button_state_t state;
} button_t;

void button_init(button_t *button);
void button_update(button_t *button);
