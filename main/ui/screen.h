#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ui/screen_i2c.h"

typedef struct rc_s rc_t;

typedef enum {
    SCREEN_MODE_MAIN,
    SCREEN_MODE_CHANNELS,
    SCREEN_MODE_TELEMETRY,
} screen_mode_e;

typedef enum {
    SCREEN_ORIENTATION_HORIZONTAL_LEFT = 0,
    SCREEN_ORIENTATION_HORIZONTAL_RIGHT,
    SCREEN_ORIENTATION_VERTICAL,
    SCREEN_ORIENTATION_VERTICAL_UPSIDE_DOWN,

#if defined(USE_RX_SUPPORT)
    SCREEN_ORIENTATION_DEFAULT = SCREEN_ORIENTATION_HORIZONTAL_LEFT,
#else
    SCREEN_ORIENTATION_DEFAULT = SCREEN_ORIENTATION_VERTICAL,
#endif
} screen_orientation_e;

typedef enum {
    SCREEN_BRIGHTNESS_LOW,
    SCREEN_BRIGHTNESS_MEDIUM,
    SCREEN_BRIGHTNESS_HIGH,

    SCREEN_BRIGHTNESS_DEFAULT = SCREEN_BRIGHTNESS_LOW,
} screen_brightness_e;

typedef struct screen_s
{
    struct
    {
        screen_i2c_config_t cfg;
        rc_t *rc;
        bool available;
        screen_mode_e mode;
        struct
        {
            int page;
            int count;
        } telemetry;
        unsigned w;
        unsigned h;
        unsigned direction;
        char *buf;
    } internal;
} screen_t;

bool screen_init(screen_t *screen, screen_i2c_config_t *cfg, rc_t *rc);
bool screen_is_available(const screen_t *screen);
void screen_shutdown(screen_t *screen);
void screen_power_on(screen_t *screen);

bool screen_handle_press(screen_t *screen);
void screen_update(screen_t *screen);
void screen_splash(screen_t *screen);
void screen_set_orientation(screen_t *screen, screen_orientation_e orientation);
void screen_set_brightness(screen_t *screen, screen_brightness_e brightness);
bool screen_is_animating(const screen_t *screen);