#include <driver/gpio.h>

#include <driver/dac.h>
#include <driver/rtc_io.h>

#include <hal/gpio.h>

hal_err_t hal_gpio_enable(hal_gpio_t gpio)
{
    hal_err_t err;
    // Disable DAC output on GPIO25 and GPIO26. It's enabled by default and
    // can alter the output levels otherwise.
    // "DAC channel 1 is attached to GPIO25, DAC channel 2 is attached to GPIO26"
    switch (gpio)
    {
    case 25:
        if ((err = dac_output_disable(DAC_CHANNEL_1)) != HAL_ERR_NONE)
        {
            return err;
        }
        break;
    case 26:
        if ((err = dac_output_disable(DAC_CHANNEL_2)) != HAL_ERR_NONE)
        {
            return err;
        }
        break;
    }

    // See https://github.com/espressif/esp-idf/blob/master/docs/api-reference/system/sleep_modes.rst
    // Pins used for wakeup need to be manually unmapped from RTC
    if (rtc_gpio_is_valid_gpio(gpio))
    {
        if ((err = rtc_gpio_deinit(gpio)) != HAL_ERR_NONE)
        {
            return err;
        }
    }
    return HAL_ERR_NONE;
}

hal_err_t hal_gpio_set_dir(hal_gpio_t gpio, hal_gpio_dir_t dir)
{
    gpio_mode_t mode = 0;
    switch (dir)
    {
    case HAL_GPIO_DIR_INPUT:
        mode = GPIO_MODE_INPUT;
        break;
    case HAL_GPIO_DIR_OUTPUT:
        mode = GPIO_MODE_OUTPUT;
        break;
    case HAL_GPIO_DIR_BIDIR:
        mode = GPIO_MODE_INPUT_OUTPUT;
        break;
    }
    return gpio_set_direction(gpio, mode);
}

hal_err_t hal_gpio_set_pull(hal_gpio_t gpio, hal_gpio_pull_t pull)
{
    gpio_pull_mode_t pull_mode = 0;
    switch (pull)
    {
    case HAL_GPIO_PULL_NONE:
        pull_mode = GPIO_FLOATING;
        break;
    case HAL_GPIO_PULL_UP:
        pull_mode = GPIO_PULLUP_ONLY;
        break;
    case HAL_GPIO_PULL_DOWN:
        pull_mode = GPIO_PULLDOWN_ONLY;
        break;
    }
    return gpio_set_pull_mode(gpio, pull_mode);
}

hal_err_t hal_gpio_set_level(hal_gpio_t gpio, uint32_t level)
{
    return gpio_set_level(gpio, level);
}

int hal_gpio_get_level(hal_gpio_t gpio)
{
    return gpio_get_level(gpio);
}

hal_err_t hal_gpio_set_isr(hal_gpio_t gpio, hal_gpio_intr_t intr, hal_gpio_isr_t isr, const void *data)
{
    hal_err_t err;

    if (isr)
    {
        gpio_int_type_t intr_type = 0;
        switch (intr)
        {
        case HAL_GPIO_INTR_POSEDGE:
            intr_type = GPIO_INTR_POSEDGE;
            break;
        case HAL_GPIO_INTR_NEGEDGE:
            intr_type = GPIO_INTR_NEGEDGE;
            break;
        case HAL_GPIO_INTR_ANYEDGE:
            intr_type = GPIO_INTR_ANYEDGE;
            break;
        case HAL_GPIO_INTR_LOW_LEVEL:
            intr_type = GPIO_INTR_LOW_LEVEL;
            break;
        case HAL_GPIO_INTR_HIGH_LEVEL:
            intr_type = GPIO_INTR_HIGH_LEVEL;
            break;
        }
        if ((err = gpio_set_intr_type(gpio, intr_type)) != HAL_ERR_NONE)
        {
            return err;
        }
        if ((err = gpio_isr_handler_add(gpio, (gpio_isr_t)isr, (void *)data)) != HAL_ERR_NONE)
        {
            return err;
        }
    }
    else
    {
        if ((err = gpio_set_intr_type(gpio, GPIO_INTR_DISABLE)) != HAL_ERR_NONE)
        {
            return err;
        }
        if ((err = gpio_isr_handler_remove(gpio)) != HAL_ERR_NONE)
        {
            return err;
        }
    }

    return HAL_ERR_NONE;
}

char *hal_gpio_toa(hal_gpio_t gpio, char *dst, size_t size)
{
    snprintf(dst, size, "%02u", (unsigned)gpio);
    return dst;
}