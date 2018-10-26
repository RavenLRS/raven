#pragma once

#include <stdbool.h>

#include "target.h"

#include "util/time.h"

#define LED_REPEAT_NONE 0
#define LED_REPEAT_FOREVER 0xFF

#define LED_LEVEL_MIN 0
#define LED_LEVEL_CENTER 128 // GPIO-only leds will be turned on for values >= LED_LEVEL_CENTER
#define LED_LEVEL_MAX 255

#define LED_PWM_RESOLUTION_BITS 10
#define LED_PWM_MAX ((1 << LED_PWM_RESOLUTION_BITS) - 1)

#define LED_PATTERN(var, s, r) \
    static const led_pattern_t var = ((led_pattern_t){.stages = s, .count = ARRAY_COUNT(s), .repeat = r})

typedef struct led_stage_s
{
    uint16_t duration;
#if !defined(LED_USE_ONLY_GPIO)
    uint16_t fade_duration;
#endif
    uint8_t level;
} led_stage_t;

typedef struct led_pattern_s
{
    const led_stage_t *stages;
    uint8_t count;
    uint8_t repeat;
} led_pattern_t;

#if !defined(LED_USE_ONLY_GPIO)
#define LED_STAGE(l, d, f) ((led_stage_t){.duration = d, .fade_duration = f, .level = l})
#define LED_STAGE_FADE_DURATION(s) (s->fade_duration)
#else
#define LED_STAGE(l, d, f) ((led_stage_t){.duration = d, .level = l})
#define LED_STAGE_FADE_DURATION(s) 0
#endif

typedef enum
{
    LED_ID_1,
} led_id_e;

typedef enum
{
    LED_MODE_NONE,

    LED_MODE_FAILSAFE,

    LED_MODE_BIND,
    LED_MODE_BIND_WITH_REQUEST,

    LED_MODE_BOOT,

    LED_MODE_COUNT,
} led_mode_e;

void led_init(void);
void led_update(void);

void led_pause(void);
void led_resume(void);

void led_start_pattern(led_id_e led_id, const led_pattern_t *pattern);

void led_mode_set(led_mode_e mode, bool set);
void led_mode_add(led_mode_e mode);
void led_mode_remove(led_mode_e mode);
