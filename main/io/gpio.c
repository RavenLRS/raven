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
#if defined(USE_GPIO_REMAP)
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
#else
    switch (tag)
    {
    case GPIO_TAG_INPUT_BIDIR:
        FALLTHROUGH;
    case GPIO_TAG_OUTPUT_BIDIR:
        FALLTHROUGH;
    case GPIO_TAG_OUTPUT_TX:
        FALLTHROUGH;
    case GPIO_TAG_INPUT_TX:
        return TX_DEFAULT_GPIO;
    case GPIO_TAG_OUTPUT_RX:
        FALLTHROUGH;
    case GPIO_TAG_INPUT_RX:
        return RX_DEFAULT_GPIO;
    }
#endif
    return HAL_GPIO_NONE;
}

char *gpio_toa(hal_gpio_t gpio)
{
    // Keep 4 values in the buffer, so function calls with up to
    // 4 gpio_toa() arguments work.
#define GPIO_TOA_BUFFER_COUNT 4
    static char buf[HAL_GPIO_NAME_LENGTH * GPIO_TOA_BUFFER_COUNT];
    static int pos = 0;
    char *ptr = buf + (pos++ * GPIO_TOA_BUFFER_COUNT);
    hal_gpio_toa(gpio, ptr, HAL_GPIO_NAME_LENGTH);
    if (pos == GPIO_TOA_BUFFER_COUNT)
    {
        pos = 0;
    }
    return ptr;
}