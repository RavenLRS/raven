#include <assert.h>
#include <string.h>

#include <driver/uart.h>

#include <hal/gpio.h>
#include <hal/mutex.h>

#include "io/serial.h"

#include "util/macros.h"

#include "target.h"

typedef struct serial_port_s
{
    const uart_port_t port_num;
    uart_dev_t *const dev;
    const int tx_sig;
    const int rx_sig;

    serial_port_config_t config;
    bool open;
    bool in_write;
    bool uses_driver;
    uart_isr_handle_t isr_handle;
    uint8_t buf[128];
    unsigned buf_pos;
    mutex_t mutex;
} serial_port_t;

// We support 2 UART ports at maximum, ignoring UART0 since
// it's used for debugging.
static serial_port_t ports[] = {
    {.port_num = UART_NUM_1, .dev = &UART1, .tx_sig = U1TXD_OUT_IDX, .rx_sig = U1RXD_IN_IDX, .open = false, .in_write = false},
    {.port_num = UART_NUM_2, .dev = &UART2, .tx_sig = U2TXD_OUT_IDX, .rx_sig = U2RXD_IN_IDX, .open = false, .in_write = false},
};

static void serial_half_duplex_enable_rx(serial_port_t *port)
{
    // Disable TX interrupts
    port->dev->int_clr.tx_done = 1;
    port->dev->int_ena.tx_done = 0;

    // Enable RX mode
    ESP_ERROR_CHECK(gpio_set_direction(port->config.rx_pin, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(port->config.rx_pin, port->config.inverted ? GPIO_PULLDOWN_ONLY : GPIO_PULLUP_ONLY));
    gpio_matrix_in(port->config.rx_pin, port->rx_sig, false);

    // Empty RX fifo
    while (port->dev->status.rxfifo_cnt)
    {
        READ_PERI_REG(UART_FIFO_REG(port->port_num));
    }

    port->dev->conf1.rxfifo_full_thrhd = 1; // RX interrupt after 1 byte
    port->dev->int_clr.rxfifo_full = 1;
    port->dev->int_ena.rxfifo_full = 1;
}

static void serial_half_duplex_enable_tx(serial_port_t *port)
{
    // Disable RX interrupts
    port->dev->int_ena.rxfifo_full = 0;
    port->dev->int_clr.rxfifo_full = 1;

    // Enable TX
    // Map the RX pin to something else, so the data we transmit
    // doesn't get into the RX fifo.
    gpio_matrix_in(RX_UNUSED_GPIO, port->rx_sig, false);
    ESP_ERROR_CHECK(gpio_set_pull_mode(port->config.tx_pin, GPIO_FLOATING));
    ESP_ERROR_CHECK(gpio_set_level(port->config.tx_pin, 0));
    ESP_ERROR_CHECK(gpio_set_direction(port->config.tx_pin, GPIO_MODE_OUTPUT));
    gpio_matrix_out(port->config.tx_pin, port->tx_sig, false, false);

    port->dev->int_clr.tx_done = 1;
    port->dev->int_ena.tx_done = 1;
}

static void serial_isr(void *arg)
{
    serial_port_t *port = arg;
    if (port->dev->int_st.tx_done)
    {
        port->dev->int_clr.tx_done = 1;
        serial_half_duplex_enable_rx(port);
    }
    else if (port->dev->int_st.rxfifo_full)
    {
        uint32_t cnt = port->dev->status.rxfifo_cnt;
        while (cnt--)
        {
            uint8_t c = port->dev->fifo.rw_byte;
            if (port->config.byte_callback)
            {
                port->config.byte_callback(port, c, port->config.byte_callback_data);
            }
            else
            {
                mutex_lock(&port->mutex);
                if (port->buf_pos < sizeof(port->buf))
                {
                    port->buf[port->buf_pos++] = c;
                }
                mutex_unlock(&port->mutex);
            }
        }
        port->dev->int_clr.rxfifo_full = 1;
    }
}

void serial_port_do_open(serial_port_t *port)
{
    uart_parity_t parity;
    switch (port->config.parity)
    {
    case SERIAL_PARITY_DISABLE:
        parity = UART_PARITY_DISABLE;
        break;
    case SERIAL_PARITY_EVEN:
        parity = UART_PARITY_EVEN;
        break;
    case SERIAL_PARITY_ODD:
        parity = UART_PARITY_ODD;
        break;
    default:
        assert(0 && "invalid serial_parity_e");
    }

    uart_stop_bits_t stop_bits;
    switch (port->config.stop_bits)
    {
    case SERIAL_STOP_BITS_1:
        stop_bits = UART_STOP_BITS_1;
        break;
    case SERIAL_STOP_BITS_2:
        stop_bits = UART_STOP_BITS_2;
        break;
    default:
        assert(0 && "invalid serial_stop_bits_e");
    }

    uart_config_t uart_config = {
        .baud_rate = port->config.baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = parity,
        .stop_bits = stop_bits,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_ERROR_CHECK(uart_param_config(port->port_num, &uart_config));
    int tx_pin = port->config.tx_pin;
    if (tx_pin < 0)
    {
        tx_pin = TX_UNUSED_GPIO;
    }
    int rx_pin = port->config.rx_pin;
    if (rx_pin < 0)
    {
        rx_pin = RX_UNUSED_GPIO;
    }
    int rx_buffer_size = port->config.rx_buffer_size;
    // Must be 129 at least
    if (rx_buffer_size < 129)
    {
        rx_buffer_size = 129;
    }

    if (port->config.inverted)
    {
        ESP_ERROR_CHECK(uart_set_line_inverse(port->port_num, UART_INVERSE_TXD | UART_INVERSE_RXD));
    }
    if (tx_pin != rx_pin)
    {
        port->uses_driver = true;
        ESP_ERROR_CHECK(uart_driver_install(port->port_num, rx_buffer_size, port->config.tx_buffer_size, 0, NULL, 0));
        ESP_ERROR_CHECK(uart_set_pin(port->port_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    }
    else
    {
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[port->config.rx_pin], PIN_FUNC_GPIO);
        port->uses_driver = false;
        port->buf_pos = 0;
        // Half duplex, start as RX
        ESP_ERROR_CHECK(uart_isr_register(port->port_num, serial_isr, port, 0, &port->isr_handle));
        serial_half_duplex_enable_rx(port);
        // Disable pause before sending TX data while in half-duplex mode,
        // since timing is very likely critical.
        port->dev->idle_conf.tx_idle_num = 0;
    }
    port->open = true;
    port->in_write = false;
}

serial_port_t *serial_port_open(const serial_port_config_t *config)
{
    // Find a free UART
    serial_port_t *port = NULL;
    for (int ii = 0; ii < ARRAY_COUNT(ports); ii++)
    {
        if (!ports[ii].open)
        {
            port = &ports[ii];
            break;
        }
    }
    assert(port);
    mutex_open(&port->mutex);
    port->config = *config;
    serial_port_do_open(port);
    return port;
}

int serial_port_read(serial_port_t *port, void *buf, size_t size, time_ticks_t timeout)
{
    if (port->uses_driver)
    {
        return uart_read_bytes(port->port_num, buf, size, timeout);
    }
    mutex_lock(&port->mutex);
    int cpy_size = MIN(size, port->buf_pos);
    if (cpy_size > 0)
    {
        memcpy(buf, port->buf, cpy_size);
        memmove(&port->buf[0], &port->buf[cpy_size], port->buf_pos - cpy_size);
        port->buf_pos -= cpy_size;
    }
    mutex_unlock(&port->mutex);
    return cpy_size;
}

bool serial_port_begin_write(serial_port_t *port)
{
    if (!port->in_write)
    {
        if (serial_port_is_half_duplex(port))
        {
            // Half duplex mode, switch to TX mode
            serial_half_duplex_enable_tx(port);
        }
        port->in_write = true;
        return true;
    }
    return false;
}

bool serial_port_end_write(serial_port_t *port)
{
    if (port->in_write)
    {
        if (serial_port_is_half_duplex(port))
        {
            port->dev->int_ena.tx_done = 1;
        }
        port->in_write = false;
        return true;
    }
    return false;
}

int serial_port_write(serial_port_t *port, const void *buf, size_t size)
{
    bool began_write = serial_port_begin_write(port);
    int n;
    if (port->uses_driver)
    {
        n = uart_write_bytes(port->port_num, buf, size);
    }
    else
    {
        port->dev->int_clr.tx_done = 1;
        port->dev->int_ena.tx_done = 0;
        const uint8_t *ptr = buf;
        for (unsigned ii = 0; ii < size; ii++)
        {
            WRITE_PERI_REG(UART_FIFO_AHB_REG(port->port_num), ptr[ii]);
        }
        n = size;
    }
    if (began_write)
    {
        serial_port_end_write(port);
    }
    return n;
}

bool serial_port_set_baudrate(serial_port_t *port, uint32_t baudrate)
{
    ESP_ERROR_CHECK(uart_set_baudrate(port->port_num, baudrate));
    return true;
}

bool serial_port_set_inverted(serial_port_t *port, bool inverted)
{
    uint32_t inverse_mask = inverted ? UART_INVERSE_TXD | UART_INVERSE_RXD : 0;
    ESP_ERROR_CHECK(uart_set_line_inverse(port->port_num, inverse_mask));
    return true;
}

void serial_port_close(serial_port_t *port)
{
    assert(port->open);
    if (port->uses_driver)
    {
        ESP_ERROR_CHECK(uart_driver_delete(port->port_num));
    }
    else
    {
        ESP_ERROR_CHECK(esp_intr_free(port->isr_handle));
    }
    mutex_close(&port->mutex);
    port->open = false;
}

bool serial_port_is_half_duplex(const serial_port_t *port)
{
    return port->config.tx_pin == port->config.rx_pin;
}

serial_half_duplex_mode_e serial_port_half_duplex_mode(const serial_port_t *port)
{
    if (serial_port_is_half_duplex(port))
    {
        if (port->dev->int_ena.tx_done == 1)
        {
            return SERIAL_HALF_DUPLEX_MODE_TX;
        }
        return SERIAL_HALF_DUPLEX_MODE_RX;
    }
    return SERIAL_HALF_DUPLEX_MODE_NONE;
}

void serial_port_set_half_duplex_mode(serial_port_t *port, serial_half_duplex_mode_e mode)
{
    assert(serial_port_is_half_duplex(port));
    {
        switch (mode)
        {
        case SERIAL_HALF_DUPLEX_MODE_NONE:
            break;
        case SERIAL_HALF_DUPLEX_MODE_RX:
            serial_half_duplex_enable_rx(port);
            break;
        case SERIAL_HALF_DUPLEX_MODE_TX:
            serial_half_duplex_enable_tx(port);
            break;
        }
    }
}

void serial_port_destroy(serial_port_t **port)
{
    if (*port)
    {
        serial_port_close(*port);
        *port = NULL;
    }
}

io_flags_t serial_port_io_flags(serial_port_t *port)
{
    if (serial_port_is_half_duplex(port))
    {
        return IO_FLAG_HALF_DUPLEX;
    }
    return 0;
}
