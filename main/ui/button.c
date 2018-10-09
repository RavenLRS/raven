#include "target.h"

#if defined(USE_TOUCH_BUTTON)
#include <driver/touch_pad.h>
#endif

#include <hal/log.h>

#include "ui/button.h"

#include "util/time.h"

static const char *TAG = "button";

#define LONG_PRESS_INTERVAL MILLIS_TO_TICKS(300)
#define REALLY_LONG_PRESS_INTERVAL MILLIS_TO_TICKS(3000)

#if defined(USE_TOUCH_BUTTON)
static int touch_pad_num_from_gpio(hal_gpio_t gpio)
{
    // See touch_pad_t
    switch (gpio)
    {
    case TOUCH_PAD_NUM0_GPIO_NUM:
        return TOUCH_PAD_NUM0;
    case TOUCH_PAD_NUM1_GPIO_NUM:
        return TOUCH_PAD_NUM1;
    case TOUCH_PAD_NUM2_GPIO_NUM:
        return TOUCH_PAD_NUM2;
    case TOUCH_PAD_NUM3_GPIO_NUM:
        return TOUCH_PAD_NUM3;
    case TOUCH_PAD_NUM4_GPIO_NUM:
        return TOUCH_PAD_NUM4;
    case TOUCH_PAD_NUM5_GPIO_NUM:
        return TOUCH_PAD_NUM5;
    case TOUCH_PAD_NUM6_GPIO_NUM:
        return TOUCH_PAD_NUM6;
    case TOUCH_PAD_NUM7_GPIO_NUM:
        return TOUCH_PAD_NUM7;
    case TOUCH_PAD_NUM8_GPIO_NUM:
        return TOUCH_PAD_NUM8;
    case TOUCH_PAD_NUM9_GPIO_NUM:
        return TOUCH_PAD_NUM9;
    }
    return -1;
}
#endif

static bool button_is_down(button_t *button)
{
#if defined(USE_TOUCH_BUTTON)
    if (button->is_touch)
    {
        uint16_t touch_value;
        uint16_t touch_filter_value;
        int touch_num = touch_pad_num_from_gpio(button->gpio);
        ESP_ERROR_CHECK(touch_pad_read(touch_num, &touch_value));
        ESP_ERROR_CHECK(touch_pad_read_filtered(touch_num, &touch_filter_value));
        return touch_value < 2100;
    }
#endif
    return hal_gpio_get_level(button->gpio) == HAL_GPIO_LOW;
}

static void button_gpio_init(button_t *button)
{
#if defined(USE_TOUCH_BUTTON)
    if (button->is_touch)
    {
        int touch_num = touch_pad_num_from_gpio(button->gpio);
        assert(touch_num >= 0);
        // Enable touch only on this pin
        ESP_ERROR_CHECK(touch_pad_clear_group_mask(TOUCH_PAD_BIT_MASK_MAX, TOUCH_PAD_BIT_MASK_MAX, TOUCH_PAD_BIT_MASK_MAX));
        ESP_ERROR_CHECK(touch_pad_init());
        ESP_ERROR_CHECK(touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V));
        ESP_ERROR_CHECK(touch_pad_filter_start(10));
        ESP_ERROR_CHECK(touch_pad_config(touch_num, 0));
        return;
    }
#endif
    HAL_ERR_ASSERT_OK(hal_gpio_setup(button->gpio, HAL_GPIO_DIR_INPUT, HAL_GPIO_PULL_UP));
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
    char name[8];

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
                hal_gpio_toa(button->gpio, name, sizeof(name));
                LOG_I(TAG, "Long press in %s", name);
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
                    hal_gpio_toa(button->gpio, name, sizeof(name));
                    LOG_I(TAG, "Really long press in %s", name);
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
            hal_gpio_toa(button->gpio, name, sizeof(name));
            LOG_I(TAG, "Short press in %s", name);
            if (button->press_callback)
            {
                button->press_callback(button->user_data);
            }
        }
    }
    button->state.is_down = is_down;
}
