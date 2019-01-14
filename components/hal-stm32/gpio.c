#include <hal/gpio.h>
#include <hal/log.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

LOG_TAG_DECLARE("GPIO");

#define HAL_GPIO_PORT(gpio) (gpio >> 5)
#define HAL_GPIO_NUMBER(gpio) (gpio & 0x1f)

static int hal_gpio_rcc(hal_gpio_t gpio)
{
    switch (HAL_GPIO_PORT(gpio))
    {
    case HAL_GPIO_PORT_A:
        return RCC_GPIOA;
    case HAL_GPIO_PORT_B:
        return RCC_GPIOB;
    case HAL_GPIO_PORT_C:
        return RCC_GPIOC;
    case HAL_GPIO_PORT_D:
        return RCC_GPIOD;
    case HAL_GPIO_PORT_E:
        return RCC_GPIOE;
    case HAL_GPIO_PORT_F:
        return RCC_GPIOF;
    }
    return 0;
}

static uint32_t hal_gpio_port(hal_gpio_t gpio)
{
    switch (HAL_GPIO_PORT(gpio))
    {
    case HAL_GPIO_PORT_A:
        return GPIOA;
    case HAL_GPIO_PORT_B:
        return GPIOB;
    case HAL_GPIO_PORT_C:
        return GPIOC;
    case HAL_GPIO_PORT_D:
        return GPIOD;
    case HAL_GPIO_PORT_E:
        return GPIOE;
    case HAL_GPIO_PORT_F:
        return GPIOF;
    }
    return 0;
}

static unsigned hal_gpio_bit(hal_gpio_t gpio)
{
    return 1 << HAL_GPIO_NUMBER(gpio);
}

hal_err_t hal_gpio_setup(hal_gpio_t gpio, hal_gpio_dir_t dir, hal_gpio_pull_t pull)
{
    rcc_periph_clock_enable(hal_gpio_rcc(gpio));

    uint8_t mode = 0;
    uint8_t cnf = 0;
    // Use output at 2MHz. We use the GPIO HAL API to reset peripherals and to
    // control buttons and buzzers, so it should be fast enough while saving a
    // bit of energy.
    switch (dir)
    {
    case HAL_GPIO_DIR_INPUT:
        mode = GPIO_MODE_INPUT;
        if (pull == HAL_GPIO_PULL_NONE)
        {
            cnf = GPIO_CNF_INPUT_FLOAT;
        }
        else
        {
            cnf = GPIO_CNF_INPUT_PULL_UPDOWN;
        }
        break;
    case HAL_GPIO_DIR_OUTPUT:
        mode = GPIO_MODE_OUTPUT_2_MHZ;
        cnf = GPIO_CNF_OUTPUT_PUSHPULL;
        break;
    case HAL_GPIO_DIR_OUTPUT_OD:
        mode = GPIO_MODE_OUTPUT_2_MHZ;
        cnf = GPIO_CNF_OUTPUT_OPENDRAIN;
        break;
    case HAL_GPIO_DIR_BIDIR:
        LOG_F(TAG, "HAL_GPIO_DIR_BIDIR not supported");
        break;
    }
    uint32_t port = hal_gpio_port(gpio);
    unsigned bit = hal_gpio_bit(gpio);
    gpio_set_mode(port, mode, cnf, bit);
    if (mode == GPIO_MODE_INPUT && cnf == GPIO_CNF_INPUT_PULL_UPDOWN)
    {
        switch (pull)
        {
        case HAL_GPIO_PULL_NONE:
            break;
        case HAL_GPIO_PULL_UP:
            gpio_set(port, bit);
            break;
        case HAL_GPIO_PULL_DOWN:
            gpio_clear(port, bit);
            break;
        case HAL_GPIO_PULL_BOTH:
            LOG_F(TAG, "HAL_GPIO_PULL_BOTH not supported")
            break;
        }
    }
    return HAL_ERR_NONE;
}

hal_err_t hal_gpio_set_level(hal_gpio_t gpio, uint32_t level)
{
    if (level > 0)
    {
        gpio_set(hal_gpio_port(gpio), hal_gpio_bit(gpio));
    }
    else
    {
        gpio_clear(hal_gpio_port(gpio), hal_gpio_bit(gpio));
    }
    return HAL_ERR_NONE;
}

int hal_gpio_get_level(hal_gpio_t gpio)
{
    uint8_t bit = hal_gpio_bit(gpio);
    uint16_t mask = gpio_get(hal_gpio_port(gpio), bit);
    return (mask & bit) ? 1 : 0;
}

hal_err_t hal_gpio_set_isr(hal_gpio_t gpio, hal_gpio_intr_t intr, hal_gpio_isr_t isr, const void *data)
{
    // TODO
    return HAL_ERR_NONE;
}

char *hal_gpio_toa(hal_gpio_t gpio, char *dst, size_t size)
{
    char port = 'A' + hal_gpio_rcc(gpio);
    snprintf(dst, size, "%c%02u", port, HAL_GPIO_NUMBER(gpio));
    return dst;
}