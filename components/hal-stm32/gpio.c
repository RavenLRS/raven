#include <assert.h>
#include <stdio.h>

#include <hal/gpio.h>
#include <hal/log.h>

#include <libopencm3/stm32/exti.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>

LOG_TAG_DECLARE("GPIO");

#define HAL_GPIO_PORT(gpio) (gpio >> 5)
#define HAL_GPIO_NUMBER(gpio) (gpio & 0x1f)

#define HAL_GPIO_EXTI_COUNT 16

typedef struct hal_gpio_isr_callback_s
{
    hal_gpio_isr_t isr;
    void *data;
} hal_gpio_isr_callback_t;

static hal_gpio_isr_callback_t isr_callbacks[HAL_GPIO_EXTI_COUNT];

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

uint32_t hal_gpio_port(hal_gpio_t gpio)
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

unsigned hal_gpio_bit(hal_gpio_t gpio)
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
    uint32_t port = hal_gpio_port(gpio);
    unsigned bit = hal_gpio_bit(gpio);
    if (level > 0)
    {
        gpio_set(port, bit);
    }
    else
    {
        gpio_clear(port, bit);
    }
    return HAL_ERR_NONE;
}

int hal_gpio_get_level(hal_gpio_t gpio)
{
    uint8_t bit = hal_gpio_bit(gpio);
    uint16_t mask = gpio_get(hal_gpio_port(gpio), bit);
    return (mask & bit) ? 1 : 0;
}

hal_err_t hal_gpio_set_isr(hal_gpio_t gpio, hal_gpio_intr_t intr, hal_gpio_isr_t isr, void *data)
{
    uint32_t port = hal_gpio_port(gpio);
    unsigned number = HAL_GPIO_NUMBER(gpio);
    assert(number < HAL_GPIO_EXTI_COUNT);
    unsigned bit = hal_gpio_bit(gpio);
    // EXTIn is the same value as GPIOn for up to 31
    if (isr)
    {
        if (isr_callbacks[number].isr)
        {
            return HAL_ERR_BUSY;
        }
        enum exti_trigger_type trigger = -1;
        switch (intr)
        {
        case HAL_GPIO_INTR_POSEDGE:
            trigger = EXTI_TRIGGER_RISING;
            break;
        case HAL_GPIO_INTR_NEGEDGE:
            trigger = EXTI_TRIGGER_FALLING;
            break;
        case HAL_GPIO_INTR_ANYEDGE:
            trigger = EXTI_TRIGGER_BOTH;
            break;
        case HAL_GPIO_INTR_LOW_LEVEL:
            return HAL_ERR_INVALID_ARG;
        case HAL_GPIO_INTR_HIGH_LEVEL:
            return HAL_ERR_INVALID_ARG;
        }
        isr_callbacks[number].isr = isr;
        isr_callbacks[number].data = data;
        exti_select_source(bit, port);
        exti_set_trigger(bit, trigger);
        exti_enable_request(bit);
    }
    else
    {
        isr_callbacks[number].isr = NULL;
        exti_disable_request(bit);
    }
    return HAL_ERR_NONE;
}

char *hal_gpio_toa(hal_gpio_t gpio, char *dst, size_t size)
{
    char port = 'A' + hal_gpio_rcc(gpio);
    snprintf(dst, size, "%c%02u", port, HAL_GPIO_NUMBER(gpio));
    return dst;
}

static void exti_isr(void)
{
    for (int ii = 0; ii < HAL_GPIO_EXTI_COUNT; ii++)
    {
        uint32_t exti = 1 << ii;
        if (exti_get_flag_status(exti))
        {
            isr_callbacks[ii].isr(isr_callbacks[ii].data);
            exti_reset_request(exti);
        }
    }
}

#define ISR_HANDLER(n) \
    void exti##n##_isr(void) { exti_isr(); }

ISR_HANDLER(0);
ISR_HANDLER(1);
ISR_HANDLER(2);
ISR_HANDLER(3);
ISR_HANDLER(4);
ISR_HANDLER(9_5);
ISR_HANDLER(15_10);
