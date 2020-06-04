#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <hal/gpio.h>

#include "util/time.h"

#define BEEPER_PWM_RESOLUTION_BITS 10
#define BEEPER_PWM_MAX ((1 << BEEPER_PWM_RESOLUTION_BITS) - 1)
#define BEEPER_PWM_DEFAULT_DUTY (BEEPER_PWM_MAX / 2)

typedef enum
{
    BEEPER_MODE_NONE,
    BEEPER_MODE_BIND,
    BEEPER_MODE_FAILSAFE,
    BEEPER_MODE_STARTUP,

    BEEPER_MODE_COUNT,
} beeper_mode_e;

typedef struct beeper_s
{
    hal_gpio_t gpio;
    beeper_mode_e mode;
    struct
    {
        bool single_beep;
        time_ticks_t next_update;
        const void *pattern;
        uint8_t pattern_state;
        uint8_t pattern_repeat;
    } internal;
} beeper_t;

void beeper_init(beeper_t *beeper, hal_gpio_t gpio);
void beeper_update(beeper_t *beeper);
void beeper_beep(beeper_t *beeper);
void beeper_set_mode(beeper_t *beeper, beeper_mode_e mode);
