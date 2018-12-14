#include "target.h"

#include <hal/log.h>

#include "ui/button.h"

#include "util/time.h"

static const char *TAG = "button";

#define LONG_PRESS_INTERVAL MILLIS_TO_TICKS(300)
#define REALLY_LONG_PRESS_INTERVAL MILLIS_TO_TICKS(3000)

static bool button_is_down(button_t *button)
{
    return hal_gpio_get_level(button->cfg.gpio) == HAL_GPIO_LOW;
}

static void button_gpio_init(button_t *button)
{
    HAL_ERR_ASSERT_OK(hal_gpio_setup(button->cfg.gpio, HAL_GPIO_DIR_INPUT, HAL_GPIO_PULL_UP));
}

static void button_send_event(button_t *button, button_event_type_e evt)
{
    LOG_I(TAG, "Event %d in button %d", (int)evt, (int)button->id);
    button_callback_f callback = button->callback;
    if (callback)
    {
        button_event_t ev = {
            .button = button,
            .type = evt,
        };
        callback(&ev, button->user_data);
    }
}

void button_init(button_t *button)
{
    button_gpio_init(button);

    button->state.is_down = button_is_down(button);
    button->state.ignore = button->state.is_down;
    button->state.long_press_sent = false;
    button->state.down_since = 0;
}

void button_update(button_t *button)
{
    bool is_down = button_is_down(button);
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
                button_send_event(button, BUTTON_EVENT_TYPE_LONG_PRESS);
                button->state.long_press_sent = true;
            }
            if (button->state.down_since + REALLY_LONG_PRESS_INTERVAL < now)
            {
                if (!button->state.really_long_press_sent)
                {
                    button_send_event(button, BUTTON_EVENT_TYPE_REALLY_LONG_PRESS);
                    button->state.really_long_press_sent = true;
                }
            }
        }
    }
    else
    {
        if (button->state.is_down && !button->state.long_press_sent && !button->state.really_long_press_sent)
        {
            button_send_event(button, BUTTON_EVENT_TYPE_SHORT_PRESS);
        }
    }
    button->state.is_down = is_down;
}
