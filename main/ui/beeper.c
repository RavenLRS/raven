#include "target.h"

#if defined(USE_BEEPER)

#if defined(BEEPER_USE_PWM)
#include <hal/pwm.h>
#endif

#include "beeper.h"

#define BEEP_LENGTH_TO_TICKS(t) MILLIS_TO_TICKS(t * 10)
#define AFTER_BEEP_LENGTH(t) (time_ticks_now() + BEEP_LENGTH_TO_TICKS(t))

#define SILENCE_REPEAT_STOP UINT16_MAX

typedef enum
{
    BEEPER_PATTERN_STATE_BEEP,
    BEEPER_PATTERN_STATE_SILENCE,
    BEEPER_PATTERN_STATE_SILENCE_REPEAT,
} beeper_pattern_state_e;

typedef struct beep_pattern_s
{
    uint8_t beep_length;     // Beep length (in 0.01s)
    uint8_t silence_length;  // Silence length after each beep (in 0.01s)
    uint8_t repeat;          // Number of repetitions
    uint16_t silence_repeat; // Silence after "repeat" repetitions (in 0.01s)
} beep_pattern_t;

static const beep_pattern_t patterns[BEEPER_MODE_COUNT - 1] = {
    // BEEPER_MODE_BIND
    {
        .beep_length = 1,
        .silence_length = 40,
        .repeat = 3,
        .silence_repeat = 300,
    },
    // BEEPER_MODE_FAILSAFE
    {
        .beep_length = 50,
        .silence_length = 50,
        .repeat = 0,
        .silence_repeat = 0,
    },
    // BEEPER_MODE_STARTUP
    {
        .beep_length = 5,
        .silence_length = 10,
        .repeat = 2,
        .silence_repeat = SILENCE_REPEAT_STOP,
    },
};

static void beeper_gpio_init(beeper_t *beeper)
{
#if defined(BEEPER_USE_PWM)
    HAL_ERR_ASSERT_OK(hal_pwm_init());
#if !defined(BEEPER_PWM_RESONANT_FREQUENCY)
#define BEEPER_PWM_RESONANT_FREQUENCY 440
#endif
    HAL_ERR_ASSERT_OK(hal_pwm_open(beeper->gpio, BEEPER_PWM_RESONANT_FREQUENCY, BEEPER_PWM_RESOLUTION_BITS));
#else
    HAL_ERR_ASSERT_OK(hal_gpio_setup(beeper->gpio, HAL_GPIO_DIR_OUTPUT, HAL_GPIO_PULL_NONE));
#endif
}

static void beeper_on(beeper_t *beeper)
{
#if defined(BEEPER_USE_PWM)
    HAL_ERR_ASSERT_OK(hal_pwm_set_duty(beeper->gpio, BEEPER_PWM_DEFAULT_DUTY));
#else
    HAL_ERR_ASSERT_OK(hal_gpio_set_level(beeper->gpio, HAL_GPIO_HIGH));
#endif
}

static void beeper_off(beeper_t *beeper)
{
#if defined(BEEPER_USE_PWM)
    HAL_ERR_ASSERT_OK(hal_pwm_set_duty(beeper->gpio, 0));
#else
    HAL_ERR_ASSERT_OK(hal_gpio_set_level(beeper->gpio, HAL_GPIO_LOW));
#endif
}

static void beeper_begin_mode(beeper_t *beeper, beeper_mode_e mode)
{
    beeper->internal.single_beep = false;
    const beep_pattern_t *pattern = &patterns[mode - 1];
    beeper->internal.pattern_state = BEEPER_PATTERN_STATE_BEEP;
    beeper->internal.pattern_repeat = 0;
    beeper->internal.pattern = pattern;
    beeper->internal.next_update = AFTER_BEEP_LENGTH(pattern->beep_length);
    beeper_on(beeper);
}

static void beeper_set_mode_force(beeper_t *beeper, beeper_mode_e mode, bool force)
{
    if (!force && ((beeper->mode > mode && mode != BEEPER_MODE_NONE) || beeper->mode == mode))
    {
        return;
    }

    beeper->mode = mode;
    if (mode == BEEPER_MODE_NONE)
    {
        beeper->internal.next_update = 0;
        beeper_off(beeper);
    }
    else
    {
        beeper_begin_mode(beeper, mode);
    }
}

void beeper_init(beeper_t *beeper, hal_gpio_t gpio)
{
    beeper->gpio = gpio;
    beeper->mode = BEEPER_MODE_NONE;
    beeper_gpio_init(beeper);
    beeper_off(beeper);
}

void beeper_update(beeper_t *beeper)
{
    if (beeper->internal.next_update == 0)
    {
        return;
    }
    time_ticks_t now = time_ticks_now();
    if (now < beeper->internal.next_update)
    {
        return;
    }
    if (beeper->internal.single_beep)
    {
        beeper->internal.single_beep = false;
        beeper_set_mode_force(beeper, beeper->mode, true);
        return;
    }

    const beep_pattern_t *pattern = beeper->internal.pattern;

    switch ((beeper_pattern_state_e)beeper->internal.pattern_state)
    {
    case BEEPER_PATTERN_STATE_BEEP:
        beeper_off(beeper);
        beeper->internal.pattern_state = BEEPER_PATTERN_STATE_SILENCE;
        beeper->internal.next_update = AFTER_BEEP_LENGTH(pattern->silence_length);
        break;
    case BEEPER_PATTERN_STATE_SILENCE:
        if (++beeper->internal.pattern_repeat >= pattern->repeat)
        {
            if (pattern->silence_repeat == SILENCE_REPEAT_STOP)
            {
                beeper_set_mode(beeper, BEEPER_MODE_NONE);
                break;
            }
            beeper->internal.pattern_state = BEEPER_PATTERN_STATE_SILENCE_REPEAT;
            beeper->internal.pattern_repeat = 0;
            beeper->internal.next_update = AFTER_BEEP_LENGTH(pattern->silence_repeat);
            break;
        }
        beeper_on(beeper);
        beeper->internal.pattern_state = BEEPER_PATTERN_STATE_BEEP;
        beeper->internal.next_update = AFTER_BEEP_LENGTH(pattern->beep_length);
        break;
    case BEEPER_PATTERN_STATE_SILENCE_REPEAT:
        // Silence finished, start pattern again
        beeper_on(beeper);
        beeper->internal.pattern_state = BEEPER_PATTERN_STATE_BEEP;
        beeper->internal.next_update = AFTER_BEEP_LENGTH(pattern->beep_length);
        break;
    }
}

void beeper_beep(beeper_t *beeper)
{
    if (beeper->mode != BEEPER_MODE_NONE)
    {
        return;
    }
    beeper->internal.single_beep = true;
    beeper->internal.next_update = time_ticks_now() + MILLIS_TO_TICKS(30);
    beeper_on(beeper);
}

void beeper_set_mode(beeper_t *beeper, beeper_mode_e mode)
{
    beeper_set_mode_force(beeper, mode, false);
}

#endif
