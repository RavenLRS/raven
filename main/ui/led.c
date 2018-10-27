#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Include this first for the LED_USE_* definitions
#include "ui/led.h"

#include <hal/gpio.h>

#if defined(LED_USE_PWM)
#include <hal/pwm.h>
#endif

#include "util/macros.h"
#include "util/time.h"

#define ROUND_AWAY_FROM_ZERO(x) ({ \
    float __x = x;                 \
    if (__x < 0)                   \
    {                              \
        __x = floorf(__x);         \
    }                              \
    else                           \
    {                              \
        __x = ceilf(__x);          \
    }                              \
    __x;                           \
})
#define UPDATE_COLOR(cur, target, step)                               \
    do                                                                \
    {                                                                 \
        cur += step;                                                  \
        if ((step > 0 && cur > target) || (step < 0 && cur < target)) \
        {                                                             \
            cur = target;                                             \
        }                                                             \
    } while (0)

#if defined(LED_USE_WS2812)
#define LED_STAGE_COLOR(s) (&s->color)
#else
#define hal_ws2812_color_t void
#define LED_STAGE_COLOR(s) NULL
#endif

#if defined(LED_USE_FADING)
#define LED_STAGE_FADE_DURATION(s) (s->fade_duration)
#else
#define LED_STAGE_FADE_DURATION(s) 0
#endif

// Patterns
static const led_stage_t none_stages[] = {
    LED_STAGE_OFF(0, 0),
};
LED_PATTERN(none_pattern, none_stages, LED_REPEAT_NONE);

static const led_stage_t failsafe_stages[] = {
    LED_STAGE(255, HAL_WS2812_RED, 50, 0),
    LED_STAGE_OFF(50, 0),
};
LED_PATTERN(failsafe_pattern, failsafe_stages, LED_REPEAT_FOREVER);

static const led_stage_t bind_stages[] = {
    LED_STAGE(255, HAL_WS2812_BLUE, 500, 150),
    LED_STAGE_OFF(500, 150),
};
LED_PATTERN(bind_pattern, bind_stages, LED_REPEAT_FOREVER);

static const led_stage_t bind_with_request_stages[] = {
    LED_STAGE(255, HAL_WS2812_BLUE, 150, 150),
    LED_STAGE_OFF(200, 0),
    LED_STAGE(255, HAL_WS2812_BLUE, 150, 150),
    LED_STAGE_OFF(1000, 150),
};
LED_PATTERN(bind_with_request_pattern, bind_with_request_stages, LED_REPEAT_FOREVER);

static const led_stage_t boot_stages[] = {
    LED_STAGE(255, HAL_WS2812_WHITE, 3000, 3000),
    LED_STAGE_OFF(500, 150),
    LED_STAGE(255, HAL_WS2812_WHITE, 50, 0),
    LED_STAGE_OFF(50, 0),
    LED_STAGE(255, HAL_WS2812_WHITE, 50, 0),
    LED_STAGE_OFF(50, 0),
    LED_STAGE(255, HAL_WS2812_WHITE, 50, 0),
    LED_STAGE_OFF(200, 0),
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
#if defined(LED_USE_PWM)
    bool use_pwm;
#endif
#if defined(LED_USE_WS2812)
    bool use_ws2812;
    struct
    {
        bool active;              // Wether fading is active
        time_ticks_t next_update; // Next update for fading
    } fading;
    struct
    {
        hal_ws2812_color_t current; // Current LED color
        hal_ws2812_color_t target;  // Color we want to transition to
        hal_ws2812_color_step_t step;
    } colors;
#endif
} led_t;

#if defined(LED_1_GPIO)
static led_t led1 = {
    .gpio = LED_1_GPIO,
#if defined(LED_1_USE_WS2812)
    .use_ws2812 = true,
#elif defined(LED_1_USE_PWM)
    .use_pwm = true,
#endif
};
#endif

#if defined(USE_LED)

#if defined(LED_USE_WS2812)
static void led_update_fading(led_t *led)
{
    hal_ws2812_color_t *current = &led->colors.current;
    hal_ws2812_color_t *target = &led->colors.target;
    if (current->r != target->r || current->g != target->g || current->b != target->b)
    {
        hal_ws2812_color_step_t *step = &led->colors.step;
        UPDATE_COLOR(current->r, target->r, step->r);
        UPDATE_COLOR(current->g, target->g, step->g);
        UPDATE_COLOR(current->b, target->b, step->b);

        HAL_ERR_ASSERT_OK(hal_ws2812_set_colors(led->gpio, current, 1));

        // We update on each tick
        led->fading.next_update++;
    }
    else
    {
        led->fading.active = false;
    }
}
#endif

static void led_set_level(led_t *led, uint8_t level, const hal_ws2812_color_t *color, unsigned fade_ms)
{
#if defined(LED_USE_WS2812)
    if (led->use_ws2812)
    {
        hal_ws2812_color_t c;
        if (color)
        {
            c.r = color->r * level / LED_LEVEL_MAX;
            c.g = color->g * level / LED_LEVEL_MAX;
            c.b = color->b * level / LED_LEVEL_MAX;
        }
        else
        {
            c.r = c.g = c.b = HAL_WS2812_COLOR_LEVEL_MAX * level / LED_LEVEL_MAX;
        }
        float cycles = MILLIS_TO_TICKS(fade_ms);
        if (cycles > 0)
        {
            if (led->fading.active)
            {
                // There's a fade in progress, jump to target color
                memcpy(&led->colors.current, &led->colors.target, sizeof(led->colors.current));
                HAL_ERR_ASSERT_OK(hal_ws2812_set_colors(led->gpio, &led->colors.current, 1));
            }

            memcpy(&led->colors.target, &c, sizeof(led->colors.target));

            int16_t delta_r = led->colors.target.r - led->colors.current.r;
            int16_t delta_g = led->colors.target.g - led->colors.current.g;
            int16_t delta_b = led->colors.target.b - led->colors.current.b;

            led->colors.step.r = CONSTRAIN(ROUND_AWAY_FROM_ZERO(delta_r / cycles), HAL_WS2812_COLOR_STEP_MIN, HAL_WS2812_COLOR_STEP_MAX);
            led->colors.step.g = CONSTRAIN(ROUND_AWAY_FROM_ZERO(delta_g / cycles), HAL_WS2812_COLOR_STEP_MIN, HAL_WS2812_COLOR_STEP_MAX);
            led->colors.step.b = CONSTRAIN(ROUND_AWAY_FROM_ZERO(delta_b / cycles), HAL_WS2812_COLOR_STEP_MIN, HAL_WS2812_COLOR_STEP_MAX);

            led->fading.active = true;
            led->fading.next_update = time_ticks_now();
        }
        else
        {
            // Stop any potential fadings that might have not completed
            led->fading.active = false;
            memcpy(&led->colors.current, &c, sizeof(led->colors.current));
            HAL_ERR_ASSERT_OK(hal_ws2812_set_colors(led->gpio, &c, 1));
        }
        return;
    }
#endif

#if defined(LED_USE_PWM)
    if (led->use_pwm)
    {
        uint32_t duty = (level * LED_PWM_MAX) / LED_LEVEL_MAX;
        HAL_ERR_ASSERT_OK(hal_pwm_set_duty_fading(led->gpio, duty, fade_ms));
        return;
    }
#endif
    // GPIO only led
    HAL_ERR_ASSERT_OK(hal_gpio_set_level(led->gpio, level >= LED_LEVEL_CENTER ? HAL_GPIO_HIGH : HAL_GPIO_LOW));
}

static void led_start_stage(led_t *led)
{
    if (led->pattern && led->stage < led->pattern->count)
    {
        const led_stage_t *stage = &led->pattern->stages[led->stage];
        led_set_level(led, stage->level, LED_STAGE_COLOR(stage), LED_STAGE_FADE_DURATION(stage));
        led->next_update = time_ticks_now() + MILLIS_TO_TICKS(stage->duration);
    }
    else
    {
        led_set_level(led, 0, NULL, 0);
        led->next_update = 0;
    }
}

static void led_open_led(led_t *led)
{
#if defined(LED_USE_WS2812)
    if (led->use_ws2812)
    {
        HAL_ERR_ASSERT_OK(hal_ws2812_open(led->gpio));
        return;
    }
#endif

#if defined(LED_USE_PWM)
    if (led->use_pwm)
    {
        HAL_ERR_ASSERT_OK(hal_pwm_init());
        HAL_ERR_ASSERT_OK(hal_pwm_open(led->gpio, 1000, LED_PWM_RESOLUTION_BITS));
        return;
    }
#endif

    HAL_ERR_ASSERT_OK(hal_gpio_setup(led->gpio, HAL_GPIO_DIR_OUTPUT, HAL_GPIO_PULL_NONE));
}

static void led_init_led(led_t *led)
{
    led_open_led(led);
    led_set_level(led, 0, NULL, 0);
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
#if defined(LED_USE_WS2812)
    if (led->fading.active)
    {
        led_update_fading(led);
    }
#endif
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

bool led_is_fading(void)
{
#if defined(LED_USE_WS2812)
#if defined(LED_1_GPIO)
    if (led1.fading.active)
    {
        return true;
    }
#endif
#endif
    return false;
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