#include <hal/gpio.h>

int hal_gpio_setup(hal_gpio_t gpio, hal_gpio_cfg_t *cfg)
{
    int ret;

    if ((ret = hal_gpio_enable(gpio)) < 0)
    {
        return ret;
    }

    if ((ret = hal_gpio_set_dir(gpio, cfg->dir)) < 0)
    {
        return ret;
    }
    if ((ret = hal_gpio_set_pull(gpio, cfg->pull)) < 0)
    {
        return ret;
    }
    return 0;
}
