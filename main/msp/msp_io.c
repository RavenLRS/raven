#include "msp_io.h"

void msp_io_set_transport(msp_io_t *io, msp_transport_t *transport)
{
    io->transport = transport;
    if (transport)
    {
        msp_conn_init(&io->conn, transport);
    }
}

void msp_io_update(msp_io_t *io)
{
    if (msp_io_is_connected(io))
    {
        msp_conn_update(&io->conn);
    }
}