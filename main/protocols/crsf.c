#include <string.h>

#include <hal/log.h>

#include "util/crc.h"

#include "crsf.h"

static const char *TAG = "CRSF";

static uint8_t crsf_frame_crc(crsf_frame_t *frame)
{
    return crc8_dvb_s2_bytes(&frame->header.type, crsf_frame_payload_size(frame) + 1);
}

const char *crsf_frame_str(crsf_frame_t *frame)
{
    // Return a string only iff there's a null byte at the end of the payload
    uint8_t size = crsf_frame_payload_size(frame);
    // Point to the 1st byte after the header (i.e. the payload)
    uint8_t *ptr = ((uint8_t *)&frame->header) + sizeof(frame->header);
    // Payload ends at size-1, since the last byte is CRC
    if (ptr[size - 1] == '\0')
    {
        return (const char *)ptr;
    }
    return NULL;
}

void crsf_frame_put_str(crsf_frame_t *frame, const char *s)
{
    size_t payload_size = s ? strlen(s) + 1 : 1;
    frame->header.frame_size = CRSF_FRAME_SIZE(payload_size);
    uint8_t *ptr = ((uint8_t *)&frame->header) + sizeof(frame->header);
    if (s)
    {
        memcpy(ptr, s, payload_size - 1);
    }
    ptr[payload_size - 1] = '\0';
}

uint8_t crsf_frame_payload_size(crsf_frame_t *frame)
{
    return frame->header.frame_size - 2;
}

uint8_t crsf_frame_total_size(crsf_frame_t *frame)
{
    return frame->header.frame_size + 2;
}

uint8_t crsf_ext_frame_payload_size(crsf_ext_frame_t *frame)
{
    // crsf_ext_header_t has 2 extra uint8 fields
    return crsf_frame_payload_size((crsf_frame_t *)frame) - (sizeof(crsf_ext_header_t) - sizeof(crsf_header_t));
}

crsf_device_info_tail_t *crsf_device_info_get_tail(crsf_device_info_t *info)
{
    for (unsigned ii = 0; ii < sizeof(info->name); ii++)
    {
        if (info->name[ii] == '\0')
        {
            return (crsf_device_info_tail_t *)&info->name[ii + 1];
        }
    }
    return NULL;
}

void crsf_port_init(crsf_port_t *port, io_t *io, crsf_frame_f frame_callback, void *callback_data)
{
    port->io = *io;
    port->frame_callback = frame_callback;
    port->callback_data = callback_data;
    port->buf_pos = 0;
}

int crsf_port_write(crsf_port_t *port, crsf_frame_t *frame)
{
    uint8_t data_size = crsf_frame_payload_size(frame);
    uint8_t *buf = (void *)frame;
    buf[sizeof(crsf_header_t) + data_size] = crsf_frame_crc(frame);
    return io_write(&port->io, buf, sizeof(crsf_header_t) + data_size + 1);
}

bool crsf_port_read(crsf_port_t *port)
{
    int rem = sizeof(port->buf) - port->buf_pos;
    int n = io_read(&port->io, &port->buf[port->buf_pos], rem, 0);
    if (n <= 0)
    {
        return false;
    }
    port->buf_pos += n;
    return crsf_port_decode(port);
}

bool crsf_port_push(crsf_port_t *port, uint8_t c)
{
    if (port->buf_pos < sizeof(port->buf))
    {
        port->buf[port->buf_pos++] = c;
        return true;
    }
    return false;
}

bool crsf_port_decode(crsf_port_t *port)
{
    int start = 0;
    int end = port->buf_pos;
    bool found = false;
    while (end - start >= 2)
    {
        int frame_length = port->buf[start + 1];
        int total_frame_size = frame_length + CRSF_FRAME_NOT_COUNTED_BYTES;
        if (end - start < total_frame_size)
        {
            // No more complete frames to decode
            break;
        }
        //LOG_I(TAG, "Got frame of size %u", total_frame_size);
        // We have a complete frame. Check checksum
        crsf_frame_t *frame = (crsf_frame_t *)&port->buf[start];
        uint8_t received_crc = port->buf[start + total_frame_size - 1];
        uint8_t expected_crc = crsf_frame_crc(frame);
        if (received_crc == expected_crc)
        {
            found = true;
            port->frame_callback(port->callback_data, frame);
        }
        else
        {
            LOG_W(TAG, "CRC error in frame with size %d: expected 0x%02x but got 0x%02x", total_frame_size, expected_crc, received_crc);
            LOG_BUFFER_W(TAG, frame, total_frame_size);
        }
        start += total_frame_size;
    }
    if (start > 0)
    {
        if ((unsigned)start > port->buf_pos)
        {
            LOG_E(TAG, "Error %d > %d", start, (int)port->buf_pos);
        }
        assert(start <= port->buf_pos);
        if ((unsigned)start != port->buf_pos)
        {
            // Move remaining data to the front
            memmove(port->buf, &port->buf[start], end - start);
        }
        port->buf_pos -= start;
    }
    return found;
}

bool crsf_port_has_buffered_data(crsf_port_t *port)
{
    return port->buf_pos > 0 && port->buf_pos < sizeof(port->buf);
}

void crsf_port_reset(crsf_port_t *port)
{
    port->buf_pos = 0;
}
