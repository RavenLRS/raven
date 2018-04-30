#pragma once

#include <stddef.h>
#include <stdint.h>

#include "msp.h"

#define MSP_TRANSPORT(tr) (&(tr)->transport)

typedef struct msp_transport_s
{
    struct
    {
        int (*read)(void *transport, msp_direction_e *direction, uint16_t *cmd, void *payload, size_t size);
        int (*write)(void *transport, msp_direction_e direction, uint16_t cmd, const void *payload, size_t size);
    } vtable;
} msp_transport_t;

int msp_transport_read(msp_transport_t *tr, msp_direction_e *direction, uint16_t *cmd, void *payload, size_t size);
int msp_transport_write(msp_transport_t *tr, msp_direction_e direction, uint16_t cmd, const void *payload, size_t size);
