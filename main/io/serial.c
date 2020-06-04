#include "io/serial.h"

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