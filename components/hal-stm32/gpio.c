#include <hal/gpio.h>

// TODO: Whole file

hal_err_t hal_gpio_enable(hal_gpio_t gpio)
{
    return HAL_ERR_NONE;
}

hal_err_t hal_gpio_set_dir(hal_gpio_t gpio, hal_gpio_dir_t dir)
{
    return HAL_ERR_NONE;
}

hal_err_t hal_gpio_set_pull(hal_gpio_t gpio, hal_gpio_pull_t pull)
{
    return HAL_ERR_NONE;
}

hal_err_t hal_gpio_set_level(hal_gpio_t gpio, uint32_t level)
{
    return HAL_ERR_NONE;
}

int hal_gpio_get_level(hal_gpio_t gpio)
{
    return 0;
}

hal_err_t hal_gpio_set_isr(hal_gpio_t gpio, hal_gpio_intr_t intr, hal_gpio_isr_t isr, const void *data)
{
    return HAL_ERR_NONE;
}

char *hal_gpio_toa(hal_gpio_t gpio, char *dst, size_t size)
{
    return dst;
}