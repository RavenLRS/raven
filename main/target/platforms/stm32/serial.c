// TODO: Whole file

#include <stdbool.h>

#include "io/serial.h"

serial_port_t *serial_port_open(const serial_port_config_t *config)
{
    return NULL;
}

int serial_port_read(serial_port_t *port, void *buf, size_t size, time_ticks_t timeout)
{
    return 0;
}

bool serial_port_begin_write(serial_port_t *port)
{
    return false;
}

bool serial_port_end_write(serial_port_t *port)
{
    return false;
}

int serial_port_write(serial_port_t *port, const void *buf, size_t size)
{
    return size;
}

bool serial_port_set_baudrate(serial_port_t *port, uint32_t baudrate)
{
    return true;
}

bool serial_port_set_inverted(serial_port_t *port, bool inverted)
{
    return true;
}

void serial_port_close(serial_port_t *port)
{
}

bool serial_port_is_half_duplex(const serial_port_t *port)
{
    return false;
}

serial_half_duplex_mode_e serial_port_half_duplex_mode(const serial_port_t *port)
{
    return SERIAL_HALF_DUPLEX_MODE_NONE;
}

void serial_port_set_half_duplex_mode(serial_port_t *port, serial_half_duplex_mode_e mode)
{
}

void serial_port_destroy(serial_port_t **port)
{
}

io_flags_t serial_port_io_flags(serial_port_t *port)
{
    return 0;
}
