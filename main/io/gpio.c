#include "config/settings.h"

#include "io/gpio.h"

#include "target.h"

hal_gpio_t gpio_get_configurable_at(unsigned idx)
{
    int c = 0;
    for (int ii = 0; ii < HAL_GPIO_MAX; ii++)
    {
        if (HAL_GPIO_IS_USER(ii))
        {
            if (c == idx)
            {
                return ii;
            }
            c++;
        }
    }
    return HAL_GPIO_NONE;
}

hal_gpio_t gpio_get_by_tag(gpio_tag_e tag)
{
    const char *key = NULL;
    switch (tag)
    {
    case GPIO_TAG_INPUT_BIDIR:
        FALLTHROUGH;
    case GPIO_TAG_INPUT_TX:
        key = SETTING_KEY_TX_TX_GPIO;
        break;
    case GPIO_TAG_INPUT_RX:
        key = SETTING_KEY_TX_RX_GPIO;
        break;
    case GPIO_TAG_OUTPUT_BIDIR:
        FALLTHROUGH;
    case GPIO_TAG_OUTPUT_TX:
        key = SETTING_KEY_RX_TX_GPIO;
        break;
    case GPIO_TAG_OUTPUT_RX:
        key = SETTING_KEY_RX_RX_GPIO;
        break;
    }
    if (key)
    {
        return settings_get_key_gpio(key);
    }
    return HAL_GPIO_NONE;
}

char *gpio_toa(hal_gpio_t gpio)
{
    static char buf[4];
    hal_gpio_toa(gpio, buf, sizeof(buf));
    return buf;
}