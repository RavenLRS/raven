#include "msp_transport.h"

int msp_transport_read(msp_transport_t *tr, msp_direction_e *direction, uint16_t *cmd, void *payload, size_t size)
{
    return tr->vtable.read(tr, direction, cmd, payload, size);
}

int msp_transport_write(msp_transport_t *tr, msp_direction_e direction, uint16_t cmd, const void *payload, size_t size)
{
    return tr->vtable.write(tr, direction, cmd, payload, size);
}
