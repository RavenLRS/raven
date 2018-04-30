#pragma once

#include <stdbool.h>

#include "msp.h"

typedef struct msp_transport_s msp_transport_t;

typedef struct msp_io_s
{
    msp_conn_t conn;
    msp_transport_t *transport;
} msp_io_t;

void msp_io_set_transport(msp_io_t *io, msp_transport_t *transport);
void msp_io_update(msp_io_t *io);
inline bool msp_io_is_connected(const msp_io_t *io) { return io->transport != NULL; }
inline msp_conn_t *msp_io_get_conn(msp_io_t *io) { return msp_io_is_connected(io) ? &io->conn : NULL; }