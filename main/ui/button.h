#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <hal/gpio.h>

#include "target.h"

typedef struct button_s button_t;

typedef enum
{
    // Single button targets use only this button
    BUTTON_ID_ENTER = 0,
#if defined(USE_BUTTON_5WAY)
    BUTTON_ID_LEFT,
    BUTTON_ID_RIGHT,
    BUTTON_ID_UP,
    BUTTON_ID_DOWN,
#endif
} button_id_e;

typedef enum
{
    BUTTON_EVENT_TYPE_SHORT_PRESS = 1,
    BUTTON_EVENT_TYPE_LONG_PRESS,
    BUTTON_EVENT_TYPE_REALLY_LONG_PRESS,
} button_event_type_e;

typedef struct button_event_s
{
    const button_t *button;
    button_event_type_e type;
} button_event_t;

#define button_event_id(e) (e->button->id)

#define BUTTON_CONFIG_FROM_GPIO(io) ((button_config_t){.gpio = io})

typedef struct button_config_s
{
    hal_gpio_t gpio;
} button_config_t;

typedef struct button_state_s
{
    bool is_down;
    bool long_press_sent;
    bool really_long_press_sent;
    unsigned long down_since;
    bool ignore; // Used when button is pressed on startup
} button_state_t;

typedef void (*button_callback_f)(const button_event_t *ev, void *user_data);

typedef struct button_s
{
    button_config_t cfg;
    button_id_e id;
    button_callback_f callback;
    void *user_data;
    button_state_t state;
} button_t;

void button_init(button_t *button);
void button_update(button_t *button);
