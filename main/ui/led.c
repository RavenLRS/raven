#include <driver/rtc_io.h>

#include <hal/gpio.h>

#include "target.h"

#include "platform/dispatch.h"

#include "util/time.h"

#include "ui/led.h"

typedef struct led_s
{
    hal_gpio_t gpio;
    bool is_initialized;
    time_ticks_t next_update;
    time_ticks_t period;
    int next_level;
} led_t;

#ifdef LED_1_GPIO
static led_t led1 = {
    .gpio = LED_1_GPIO,
};
#endif

// Guard this separately in case we add support for more than 1 led
// and this has check check for LED_1 || LED_2 || ... || LED_N
#if defined(LED_1_GPIO)

static void led_on_led(led_t *led)
{
    hal_gpio_set_level(led->gpio, HAL_GPIO_HIGH);
}

static void led_off_led(led_t *led)
{
    hal_gpio_set_level(led->gpio, HAL_GPIO_LOW);
}

static void led_init_led_task(void *data)
{
    led_t *led = data;
    led_on_led(led);
    time_millis_delay(1500);
    led_off_led(led);
    led->is_initialized = true;
}

static void led_init_led(led_t *led)
{
    hal_gpio_cfg_t cfg = {
        .dir = HAL_GPIO_DIR_OUTPUT,
        .pull = HAL_GPIO_PULL_NONE,
    };
    hal_gpio_setup(led->gpio, &cfg);
    led_off_led(led);
    led->next_update = 0;
    led->is_initialized = false;
    dispatch(led_init_led_task, led);
}

static void led_update_led(led_t *led)
{
    if (!led->is_initialized)
    {
        return;
    }
    time_ticks_t now = time_ticks_now();
    if (led->next_update > 0 && now > led->next_update)
    {
        if (led->next_level)
        {
            led_on_led(led);
        }
        else
        {
            led_off_led(led);
        }
        led->next_level = led->next_level ? 0 : 1;
        led->next_update = time_ticks_now() + led->period;
    }
}

static void led_set_blink_period_led(led_t *led, time_ticks_t period)
{
    led->period = period;
    led_off_led(led);
    if (period > 0)
    {
        led->next_update = time_ticks_now() + period;
        led->next_level = 1;
    }
    else
    {
        led->next_update = 0;
    }
}

#endif

void led_init(void)
{
#ifdef LED_1_GPIO
    led_init_led(&led1);
#endif
}

void led_update(void)
{
#ifdef LED_1_GPIO
    led_update_led(&led1);
#endif
}

void led_on(led_id_e led_id)
{
    switch (led_id)
    {
#ifdef LED_1_GPIO
    case LED_ID_1:
        led_on_led(&led1);
        break;
#endif
    default:
        break;
    }
}

void led_off(led_id_e led_id)
{
    switch (led_id)
    {
#ifdef LED_1_GPIO
    case LED_ID_1:
        led_off_led(&led1);
        break;
#endif
    default:
        break;
    }
}

void led_blink(led_id_e led_id)
{
    // Manual blink resets the blinking period
    led_set_blink_period(led_id, 0);
}

void led_set_blink_mode(led_id_e led_id, led_blink_mode_e mode)
{
    time_ticks_t period = 0;
    switch (mode)
    {
    case LED_BLINK_MODE_NONE:
        break;
    case LED_BLINK_MODE_BIND:
        period = MILLIS_TO_TICKS(500);
    }
    led_set_blink_period(led_id, period);
}

void led_set_blink_period(led_id_e led_id, time_ticks_t period)
{
    switch (led_id)
    {
#ifdef LED_1_GPIO
    case LED_ID_1:
        led_set_blink_period_led(&led1, period);
        break;
#endif
    default:
        break;
    }
}