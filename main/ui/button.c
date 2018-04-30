#include <driver/rtc_io.h>

#include <hal/log.h>

#include "ui/button.h"

#include "util/time.h"

static const char *TAG = "button";

#define LONG_PRESS_INTERVAL MILLIS_TO_TICKS(300)
#define REALLY_LONG_PRESS_INTERVAL MILLIS_TO_TICKS(3000)

void button_init(button_t *button)
{
    // See https://github.com/espressif/esp-idf/blob/master/docs/api-reference/system/sleep_modes.rst
    // Pins used for wakeup need to be manually unmapped from RTC
    rtc_gpio_deinit(button->pin);
    gpio_set_direction(button->pin, GPIO_MODE_INPUT);
    gpio_set_pull_mode(button->pin, GPIO_PULLUP_ONLY);
    button->state.is_down = gpio_get_level(button->pin) == 0;
    button->state.ignore = button->state.is_down;
    button->state.long_press_sent = false;
    button->state.down_since = 0;
}

void button_update(button_t *button)
{
    bool is_down = gpio_get_level(button->pin) == 0;
    time_ticks_t now = time_ticks_now();
    if (button->state.ignore)
    {
        if (!is_down)
        {
            button->state.ignore = false;
        }
        return;
    }
    if (is_down)
    {
        if (!button->state.is_down)
        {
            button->state.down_since = now;
            button->state.long_press_sent = false;
            button->state.really_long_press_sent = false;
        }
        else if (button->state.down_since + LONG_PRESS_INTERVAL < now)
        {
            if (!button->state.long_press_sent)
            {
                LOG_I(TAG, "Long press in %d", button->pin);
                if (button->long_press_callback)
                {
                    button->long_press_callback(button->user_data);
                }
                button->state.long_press_sent = true;
            }
            if (button->state.down_since + REALLY_LONG_PRESS_INTERVAL < now)
            {
                if (!button->state.really_long_press_sent)
                {
                    LOG_I(TAG, "Really long press in %d", button->pin);
                    if (button->really_long_press_callback)
                    {
                        button->really_long_press_callback(button->user_data);
                    }
                    button->state.really_long_press_sent = true;
                }
            }
        }
    }
    else
    {
        if (button->state.is_down && !button->state.long_press_sent && !button->state.really_long_press_sent)
        {
            LOG_I(TAG, "Short press in %d", button->pin);
            if (button->press_callback)
            {
                button->press_callback(button->user_data);
            }
        }
    }
    button->state.is_down = is_down;
}
