#pragma once

#include <stdint.h>

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
    uint8_t pin;
    void *user_data;
    void (*press_callback)(void *data);
    void (*long_press_callback)(void *data);
    void (*really_long_press_callback)(void *data);
    button_state_t state;
} button_t;

void button_init(button_t *button);
void button_update(button_t *button);
