#include "ibus.h"
#include "rc/rc_data.h"
#include <hal/log.h>
#include <string.h>

static const char *TAG = "IBUS";

static bool ibus_frame_crc_check(ibus_frame_t *frame)
{
    uint16_t sum = 0;
    for (int i = 0; i < sizeof(ibus_frame_t) - 2; i++)
    {
        sum += frame->bytes[i];
    }
    return sum + frame->payload.checksum == 0xffff;
}

void ibus_port_init(ibus_port_t *port, io_t *io, ibus_frame_f frame_callback, void *callback_data)
{
    port->io = *io;
    port->frame_callback = frame_callback;
    port->callback_data = callback_data;
    port->buf_pos = 0;
}

void ibus_port_reset(ibus_port_t *port)
{
    port->buf_pos = 0;
}

bool ibus_port_decode(ibus_port_t *port)
{
    int start = 0;
    int end = port->buf_pos;
    bool found = false;

    while (end - start >= 6)
    {
        size_t total_frame_size = port->buf[start];

        if (end - start < total_frame_size)
        {
            // No more complete frames to decode
            break;
        }
        //LOG_I(TAG, "Got frame of size %u", total_frame_size);
        // We have a complete frame. Check checksum
        ibus_frame_t *frame = (ibus_frame_t *)&port->buf[start];
        if (ibus_frame_crc_check(frame))
        {
            found = true;
            port->frame_callback(port->callback_data, frame);
        }
        else
        {
            LOG_W(TAG, "CRC error in frame with size %d.", total_frame_size);
            LOG_BUFFER_W(TAG, frame, total_frame_size);
        }
        start += total_frame_size;
    }
    if (start > 0)
    {
        if (start > port->buf_pos)
        {
            LOG_E(TAG, "Error %d > %d", start, (int)port->buf_pos);
        }
        assert(start <= port->buf_pos);
        if (start != port->buf_pos)
        {
            // Move remaining data to the front
            memmove(port->buf, &port->buf[start], end - start);
        }
        port->buf_pos -= start;
    }
    return found;
}