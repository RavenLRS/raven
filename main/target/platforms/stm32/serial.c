#include <stdbool.h>
#include <stdint.h>

#include <libopencm3/cm3/nvic.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/usart.h>

#include <hal/gpio-private.h>

#include "io/serial.h"

#include "util/macros.h"

typedef struct serial_port_data_s
{
    enum rcc_periph_clken gpio_rcc;
    enum rcc_periph_clken usart_rcc;
    hal_gpio_t tx;
    hal_gpio_t rx;
    uint8_t irqn;
} serial_port_data_t;

typedef struct serial_port_s
{
    uint32_t usart;
    bool is_open;
    serial_byte_callback_f byte_callback;
    void *byte_callback_data;
} serial_port_t;

static serial_port_t ports[] = {
    {USART1, false, NULL, NULL},
    {USART2, false, NULL, NULL},
    {USART3, false, NULL, NULL},
};

static const serial_port_data_t port_data[] = {
    {RCC_GPIOA, RCC_USART1, HAL_GPIO_PA(9), HAL_GPIO_PA(10), NVIC_USART1_IRQ},  // USART1
    {RCC_GPIOA, RCC_USART2, HAL_GPIO_PA(2), HAL_GPIO_PA(3), NVIC_USART2_IRQ},   // USART2
    {RCC_GPIOB, RCC_USART3, HAL_GPIO_PB(10), HAL_GPIO_PB(11), NVIC_USART3_IRQ}, // USART3
};

typedef enum
{
    SERIAL_PORT_OPEN_FULL_DUPLEX,
    SERIAL_PORT_OPEN_HALF_DUPLEX,
    SERIAL_PORT_OPEN_TX_ONLY,
    SERIAL_PORT_OPEN_RX_ONLY,
} serial_port_open_mode_e;

static void serial_port_set_half_duplex(serial_port_t *port, bool enabled)
{
    uint32_t usart = port->usart;
    if (enabled)
    {
        USART_CR3(usart) |= USART_CR3_HDSEL;
    }
    else
    {
        USART_CR3(usart) &= ~USART_CR3_HDSEL;
    }
}

serial_port_t *serial_port_open(const serial_port_config_t *config)
{
    serial_port_t *port = NULL;
    serial_port_open_mode_e open_mode = 0;
    for (int ii = 0; ii < ARRAY_COUNT(port_data) && !port; ii++)
    {
        const serial_port_data_t *p = &port_data[ii];
        if (config->tx == p->tx && config->rx == p->rx)
        {
            // Full duplex
            port = &ports[ii];
            open_mode = SERIAL_PORT_OPEN_FULL_DUPLEX;
        }
        else if (config->tx == p->tx && config->rx == p->tx)
        {
            // Half duplex
            port = &ports[ii];
            open_mode = SERIAL_PORT_OPEN_HALF_DUPLEX;
        }
        else if (config->tx == p->tx && config->rx == SERIAL_UNUSED_GPIO)
        {
            // TX only
            port = &ports[ii];
            open_mode = SERIAL_PORT_OPEN_TX_ONLY;
        }
        else if (config->rx == p->rx && config->tx == SERIAL_UNUSED_GPIO)
        {
            // RX only
            port = &ports[ii];
            open_mode = SERIAL_PORT_OPEN_RX_ONLY;
        }
    }

    // If we reach this point with port = NULL there's a programming error
    // because we couldn't match the pins
    ASSERT(port);

    if (port->is_open)
    {
        return NULL;
    }

    // Disable first, since some settings can only be changed
    // while the uart is disabled.
    usart_disable(port->usart);

    int port_index = port - &ports[0];
    const serial_port_data_t *p = &port_data[port_index];

    rcc_periph_clock_enable(p->gpio_rcc);
    rcc_periph_clock_enable(p->usart_rcc);

    usart_set_baudrate(port->usart, config->baud_rate);

    if (open_mode != SERIAL_PORT_OPEN_RX_ONLY)
    {
        gpio_set_mode(hal_gpio_port(config->tx), GPIO_MODE_OUTPUT_50_MHZ,
                      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, hal_gpio_bit(config->tx));
    }

    if (open_mode == SERIAL_PORT_OPEN_FULL_DUPLEX || open_mode == SERIAL_PORT_OPEN_RX_ONLY)
    {
        gpio_set_mode(hal_gpio_port(config->rx), GPIO_MODE_INPUT,
                      GPIO_CNF_INPUT_FLOAT, hal_gpio_bit(config->rx));
    }

    switch (config->parity)
    {
    case SERIAL_PARITY_DISABLE:
        usart_set_parity(port->usart, USART_PARITY_NONE);
        break;
    case SERIAL_PARITY_EVEN:
        usart_set_parity(port->usart, USART_PARITY_EVEN);
        break;
    case SERIAL_PARITY_ODD:
        usart_set_parity(port->usart, USART_PARITY_ODD);
        break;
    }

    switch (config->stop_bits)
    {
    case SERIAL_STOP_BITS_1:
        usart_set_stopbits(port->usart, USART_STOPBITS_1);
        break;
    case SERIAL_STOP_BITS_2:
        usart_set_stopbits(port->usart, USART_STOPBITS_2);
        break;
    }

    // TODO: F1 doesn't support inversion. Add a compile time
    // conditional to disable the settings.
    serial_port_set_half_duplex(port, false);

    switch (open_mode)
    {
    case SERIAL_PORT_OPEN_FULL_DUPLEX:
        usart_set_mode(port->usart, USART_MODE_TX_RX);
        break;
    case SERIAL_PORT_OPEN_HALF_DUPLEX:
        usart_set_mode(port->usart, USART_MODE_TX_RX);
        serial_port_set_half_duplex(port, true);
        break;
    case SERIAL_PORT_OPEN_TX_ONLY:
        usart_set_mode(port->usart, USART_MODE_TX);
        break;
    case SERIAL_PORT_OPEN_RX_ONLY:
        usart_set_mode(port->usart, USART_MODE_RX);
        break;
    }

    usart_set_flow_control(port->usart, USART_FLOWCONTROL_NONE);

    if (config->byte_callback)
    {
        nvic_enable_irq(p->irqn);
        usart_enable_rx_interrupt(port->usart);
    }
    else
    {
        usart_disable_rx_interrupt(port->usart);
        nvic_disable_irq(p->irqn);
    }

    port->byte_callback = config->byte_callback;
    port->byte_callback_data = config->byte_callback_data;

    port->is_open = true;

    usart_enable(port->usart);

    return port;
}

int serial_port_read(serial_port_t *port, void *buf, size_t size, time_ticks_t timeout)
{
    return 0;
    UNUSED(timeout);

    // TODO: Timeout
    uint8_t *ptr = buf;
    size_t n;
    for (n = 0; n < size; n++, ptr++)
    {
        if (!usart_get_flag(port->usart, USART_SR_RXNE))
        {
            // No data left
            break;
        }
        *ptr = usart_recv(port->usart);
    }
    return n;
}

bool serial_port_begin_write(serial_port_t *port)
{
    UNUSED(port);
    // This is used to group writes on ESP32 to avoid
    // switching the pin between TX and RX multiple times
    // when doing multiple writes that don't require a
    // response. Since STM32 takes care of this for us,
    // we don't need to group writes.
    return true;
}

bool serial_port_end_write(serial_port_t *port)
{
    UNUSED(port);
    // See serial_port_begin_write()
    return true;
}

int serial_port_write(serial_port_t *port, const void *buf, size_t size)
{
    const uint8_t *ptr = buf;
    for (size_t ii = 0; ii < size; ii++, ptr++)
    {
        usart_send_blocking(port->usart, *ptr);
    }
    return size;
}

bool serial_port_set_baudrate(serial_port_t *port, uint32_t baudrate)
{
    usart_set_baudrate(port->usart, baudrate);
    return true;
}

bool serial_port_set_inverted(serial_port_t *port, bool inverted)
{
    UNUSED(port);
    UNUSED(inverted);
    // TODO: Inversion on targets that support it
    return true;
}

void serial_port_close(serial_port_t *port)
{
    usart_disable(port->usart);
    port->is_open = false;
}

bool serial_port_is_half_duplex(const serial_port_t *port)
{
    return USART_CR3(port->usart) & USART_CR3_HDSEL;
}

serial_half_duplex_mode_e serial_port_half_duplex_mode(const serial_port_t *port)
{
    UNUSED(port);
    return SERIAL_HALF_DUPLEX_MODE_NONE;
}

void serial_port_set_half_duplex_mode(serial_port_t *port, serial_half_duplex_mode_e mode)
{
    UNUSED(port);
    UNUSED(mode);
    // STM32 automatically makes the USART_TX pin an input when it's not sending
}

static void usart_isr(serial_port_t *port)
{
    /* Check if we were called because of RXNE. */
    if (((USART_CR1(port->usart) & USART_CR1_RXNEIE) != 0) &&
        ((USART_SR(port->usart) & USART_SR_RXNE) != 0))
    {
        serial_byte_callback_f callback = port->byte_callback;
        if (callback)
        {
            uint8_t c = usart_recv(port->usart);
            callback(port, c, port->byte_callback_data);
        }
    }
}

#define USART_ISR(num) \
    void usart##num##_isr(void) { usart_isr(&ports[num - 1]); }

USART_ISR(1);
USART_ISR(2);
USART_ISR(3);