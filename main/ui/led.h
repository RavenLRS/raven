#pragma once

#include <stdbool.h>

#include "util/time.h"

typedef enum {
    LED_ID_1,
} led_id_e;

typedef enum {
    LED_BLINK_MODE_NONE,
    LED_BLINK_MODE_BIND,
} led_blink_mode_e;

void led_init(void);
void led_update(void);

void led_on(led_id_e led_id);
void led_off(led_id_e led_id);

void led_blink(led_id_e led_id);
void led_set_blink_mode(led_id_e led_id, led_blink_mode_e mode);
void led_set_blink_period(led_id_e led_id, time_ticks_t period);