#include <stdbool.h>
#include <stdint.h>

#include "target.h"

#include <hal/gpio.h>
#if !defined(LED_USE_ONLY_GPIO)
#include <hal/pwm.h>
#endif

#include "util/macros.h"
#include "util/time.h"

#include "ui/led.h"

// Patterns
static const led_stage_t none_stages[] = {
    LED_STAGE(0, 0, 0),
};
LED_PATTERN(none_pattern, none_stages, LED_REPEAT_NONE);

static const led_stage_t failsafe_stages[] = {
    LED_STAGE(255, 50, 0),
    LED_STAGE(0, 50, 0),
};
LED_PATTERN(failsafe_pattern, failsafe_stages, LED_REPEAT_FOREVER);

static const led_stage_t bind_stages[] = {
    LED_STAGE(255, 500, 150),
    LED_STAGE(0, 500, 150),
};
LED_PATTERN(bind_pattern, bind_stages, LED_REPEAT_FOREVER);

static const led_stage_t bind_with_request_stages[] = {
    LED_STAGE(255, 150, 150),
    LED_STAGE(0, 200, 0),
    LED_STAGE(255, 150, 150),
    LED_STAGE(0, 1000, 150),
};
LED_PATTERN(bind_with_request_pattern, bind_with_request_stages, LED_REPEAT_FOREVER);

static const led_stage_t boot_stages[] = {
    LED_STAGE(255, 3000, 3000),
    LED_STAGE(0, 500, 150),
    LED_STAGE(255, 50, 0),
    LED_STAGE(0, 50, 0),
    LED_STAGE(255, 50, 0),
    LED_STAGE(0, 50, 0),
    LED_STAGE(255, 50, 0),
    LED_STAGE(0, 200, 0),
};
LED_PATTERN(boot_pattern, boot_stages, 1);

static const led_pattern_t *patterns[] = {
    [LED_MODE_NONE] = &none_pattern,
    [LED_MODE_FAILSAFE] = &failsafe_pattern,
    [LED_MODE_BIND] = &bind_pattern,
    [LED_MODE_BIND_WITH_REQUEST] = &bind_with_request_pattern,
    [LED_MODE_BOOT] = &boot_pattern,
};

_Static_assert(LED_MODE_COUNT == ARRAY_COUNT(patterns), "invalid number of LED patterns");

static led_mode_e active_mode = LED_MODE_NONE;
static bool enabled_modes[LED_MODE_COUNT];

typedef struct led_s
{
    hal_gpio_t gpio;
    const led_pattern_t *pattern;
    uint8_t stage;
    uint8_t repeat;
    time_ticks_t next_update;
#if !defined(LED_USE_ONLY_GPIO)
    bool use_pwm;
#endif
} led_t;

#if defined(LED_1_GPIO)
static led_t led1 = {
    .gpio = LED_1_GPIO,
#if !defined(LED_USE_ONLY_GPIO)
    .use_pwm = true,
#endif
};
#endif

#if defined(LED_1_GPIO)

static void led_set_level(led_t *led, uint8_t level, unsigned fade_ms)
{
#if !defined(LED_USE_ONLY_GPIO)
    if (led->use_pwm)
    {
        uint32_t duty = (level * LED_PWM_MAX) / LED_LEVEL_MAX;
        HAL_ERR_ASSERT_OK(hal_pwm_set_duty_fading(led->gpio, duty, fade_ms));
    }
    else
    {
#endif
        HAL_ERR_ASSERT_OK(hal_gpio_set_level(led->gpio, level >= LED_LEVEL_CENTER ? HAL_GPIO_HIGH : HAL_GPIO_LOW));
#if !defined(LED_USE_ONLY_GPIO)
    }
#endif
}

static void led_start_stage(led_t *led)
{
    if (led->pattern && led->stage < led->pattern->count)
    {
        const led_stage_t *stage = &led->pattern->stages[led->stage];
        led_set_level(led, stage->level, LED_STAGE_FADE_DURATION(stage));
        led->next_update = time_ticks_now() + MILLIS_TO_TICKS(stage->duration);
    }
    else
    {
        led_set_level(led, 0, 0);
        led->next_update = 0;
    }
}

static void led_init_led(led_t *led)
{
#if !defined(LED_USE_ONLY_GPIO)
    if (led->use_pwm)
    {
        HAL_ERR_ASSERT_OK(hal_pwm_init());
        HAL_ERR_ASSERT_OK(hal_pwm_open(led->gpio, 1000, LED_PWM_RESOLUTION_BITS))
    }
    else
    {
#endif
        HAL_ERR_ASSERT_OK(hal_gpio_setup(led->gpio, HAL_GPIO_DIR_OUTPUT, HAL_GPIO_PULL_NONE));
#if !defined(LED_USE_ONLY_GPIO)
    }
#endif
    led_set_level(led, 0, 0);
    led->next_update = 0;
}

static void led_update_led(led_t *led)
{
    if (!led->pattern)
    {
        return;
    }
    time_ticks_t now = time_ticks_now();
    if (led->next_update > 0 && now > led->next_update)
    {
        led->stage++;
        if (led->stage >= led->pattern->count)
        {
            led->repeat++;
            if (led->pattern->repeat == LED_REPEAT_FOREVER || led->repeat < led->pattern->repeat)
            {
                // Restart
                led->stage = 0;
            }
            else
            {
                led_mode_remove(active_mode);
                return;
            }
        }
        led_start_stage(led);
    }
}

static void led_led_start_pattern(led_t *led, const led_pattern_t *pattern)
{
    led->pattern = pattern;
    led->stage = 0;
    led->repeat = 0;
    led_start_stage(led);
}

#endif

static void led_end_mode(led_mode_e mode)
{
    if (patterns[mode]->repeat != LED_REPEAT_FOREVER)
    {
        enabled_modes[mode] = false;
    }
}

static void led_start_mode(led_mode_e mode)
{
#if defined(LED_1_GPIO)
    led_led_start_pattern(&led1, patterns[mode]);
#endif
}

static void led_update_active_mode(bool force)
{
    led_mode_e new_active_mode = LED_MODE_NONE;
    for (int ii = LED_MODE_COUNT - 1; ii >= 0; ii--)
    {
        if (enabled_modes[ii])
        {
            new_active_mode = ii;
            break;
        }
    }
    if (force || new_active_mode != active_mode)
    {
        led_end_mode(active_mode);
        active_mode = new_active_mode;
        led_start_mode(new_active_mode);
    }
}

void led_init(void)
{
#if defined(LED_1_GPIO)
    led_init_led(&led1);
#endif
}

void led_update(void)
{
#if defined(LED_1_GPIO)
    led_update_led(&led1);
#endif
}

void led_pause(void)
{
#if defined(LED_1_GPIO)
    led1.pattern = NULL;
#endif
}

void led_resume(void)
{
    led_update_active_mode(true);
}

void led_start_pattern(led_id_e led_id, const led_pattern_t *pattern)
{
    switch (led_id)
    {
#if defined(LED_1_GPIO)
    case LED_ID_1:
        led_led_start_pattern(&led1, pattern);
        break;
#endif
    default:
        break;
    }
}

void led_mode_set(led_mode_e mode, bool set)
{
    enabled_modes[mode] = set;
    led_update_active_mode(false);
}

void led_mode_add(led_mode_e mode)
{
    led_mode_set(mode, true);
}

void led_mode_remove(led_mode_e mode)
{
    led_mode_set(mode, false);
}