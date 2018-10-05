#include <hal/gpio.h>

hal_err_t hal_gpio_setup(hal_gpio_t gpio, hal_gpio_cfg_t *cfg)
{
    hal_err_t err;

    if ((err = hal_gpio_enable(gpio)) != HAL_ERR_NONE)
    {
        return err;
    }

    if ((err = hal_gpio_set_dir(gpio, cfg->dir)) != HAL_ERR_NONE)
    {
        return err;
    }
    if ((err = hal_gpio_set_pull(gpio, cfg->pull)) != HAL_ERR_NONE)
    {
        return err;
    }
    return HAL_ERR_NONE;
}
