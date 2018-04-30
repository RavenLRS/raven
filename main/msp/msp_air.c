#include <string.h>

#include <hal/log.h>

#include "air/air_stream.h"

#include "util/macros.h"
#include "util/uvarint.h"

#include "msp_air.h"

static const char *TAG = "MSP.Transport.Air";

static int msp_air_read(void *transport, msp_direction_e *direction, uint16_t *cmd, void *payload, size_t size)
{
    // No read support, the air transport is implemeted by pushing
    // from input_air_t
    return MSP_EOF;
}

static int msp_air_write(void *transport, msp_direction_e direction, uint16_t cmd, const void *payload, size_t size)
{
    msp_air_t *tr = transport;
    // We need at most 3 extra bytes to encode the cmd and 1 for the direction
    uint8_t buf[size + 3 + 1];
    switch (direction)
    {
    case MSP_DIRECTION_TO_MWC:
        buf[0] = '<';
        break;
    case MSP_DIRECTION_FROM_MWC:
        buf[0] = '>';
        break;
    }
    int used = uvarint_encode16(&buf[1], sizeof(buf) - 1, cmd);
    if (payload && size > 0)
    {
        memcpy(buf + 1 + used, payload, size);
    }
    size_t feed_size = 1 + used + size;
    if (tr->before_feed)
    {
        tr->before_feed(tr, feed_size, tr->user_data);
    }
    return air_stream_feed_output_cmd(tr->air_stream, AIR_CMD_MSP, buf, feed_size);
}

static bool msp_air_decode(const void *payload, size_t size, msp_direction_e *direction, uint16_t *cmd, const void **data, size_t *data_size)
{
    const uint8_t *ptr = payload;
    switch (*ptr)
    {
    case '<':
        *direction = MSP_DIRECTION_TO_MWC;
        break;
    case '>':
        *direction = MSP_DIRECTION_FROM_MWC;
        break;
    default:
        LOG_W(TAG, "Invalid direction chracter %d (%c)", *ptr, *ptr);
        return false;
    }
    // Move past the direction character
    ptr++;
    size--;
    int used = uvarint_decode16(cmd, ptr, size);
    if (used <= 0)
    {
        return false;
    }
    *data = ptr + used;
    *data_size = size - used;
    return true;
}

void msp_air_init(msp_air_t *tr, air_stream_t *stream, msp_air_before_feed_f before_feed, void *user_data)
{
    tr->transport.vtable.read = msp_air_read;
    tr->transport.vtable.write = msp_air_write;
    tr->air_stream = stream;
    tr->before_feed = before_feed;
    tr->user_data = user_data;
}

void msp_air_dispatch(msp_air_t *tr, msp_conn_t *conn, const void *payload, size_t size)
{
    msp_direction_e direction = 0;
    uint16_t cmd = 0;
    const void *data = NULL;
    size_t data_size = 0;
    if (conn)
    {
        if (!msp_air_decode(payload, size, &direction, &cmd, &data, &data_size))
        {
            LOG_W(TAG, "Invalid MSP payload");
            LOG_BUFFER_W(TAG, payload, size);
            return;
        }
        msp_conn_dispatch_message(conn, direction, cmd, data, data_size);
    }
}