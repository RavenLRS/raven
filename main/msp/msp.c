#include <string.h>

#include <hal/log.h>

#include "msp_transport.h"

#include "msp.h"

static const char *TAG = "MSP";

void msp_conn_init(msp_conn_t *conn, msp_transport_t *transport)
{
    assert(transport);
    RING_BUFFER_INIT(&conn->rb, msp_callback_req_t, MSP_QUEUE_MAX_SIZE);
    conn->transport = transport;
    msp_conn_set_global_callback(conn, NULL, NULL);
}

void msp_conn_update(msp_conn_t *conn)
{
    int n;
    msp_direction_e direction;
    uint16_t cmd;
    uint8_t buf[MSP_MAX_PAYLOAD_SIZE];
    while ((n = msp_transport_read(conn->transport, &direction, &cmd, buf, sizeof(buf))) != MSP_EOF)
    {
        LOG_D(TAG, "Got MSP (%s MWC) code %d, payload size %d",
              direction == MSP_DIRECTION_FROM_MWC ? "from" : "to", (int)cmd, n);
        uint8_t *data = n > 0 ? buf : NULL;
        msp_conn_dispatch_message(conn, direction, cmd, data, n);
    }
}

int msp_conn_write(msp_conn_t *conn, msp_direction_e direction, uint16_t cmd, const void *payload, size_t size)
{
    return msp_transport_write(conn->transport, direction, cmd, payload, size);
}

int msp_conn_send(msp_conn_t *conn, uint16_t cmd, const void *payload, size_t size, msp_cmd_callback_f callback, void *callback_data)
{
    msp_callback_req_t cb = {
        .code = cmd,
        .callback = callback,
        .data = callback_data,
    };

    // Write before enqueing the callback, since the write could fail
    int ret = msp_conn_write(conn, MSP_DIRECTION_TO_MWC, cmd, payload, size);
    if (ret < 0)
    {
        // TODO: Handle MSP_BUSY with an internal buffer?
        return ret;
    }
    // We need to force push here because we don't support callback
    // expiration yet.
    // TODO: Support callback expiration
    if (!conn->global_callback && !ring_buffer_force_push(&conn->rb, &cb))
    {
        LOG_I(TAG, "MSP callback buffer is full");
        // Buffer is full
        if (callback)
        {
            callback(conn, cmd, NULL, -1, callback_data);
        }
        return -1;
    }
    return ret;
}

void msp_conn_dispatch_message(msp_conn_t *conn, msp_direction_e direction, uint16_t cmd, const void *data, int size)
{
    // Call the callback
    if (conn->global_callback)
    {
        if (size < 0)
        {
            LOG_W(TAG, "Got MSP error code %d, skipping global callback", size);
            return;
        }
        conn->global_callback(conn, cmd, data, size, conn->global_callback_data);

        return;
    }

    msp_callback_req_t cb_req;
    // TODO: Find a better strategy than discarding all callbacks
    // if we miss a packet
    while (ring_buffer_pop(&conn->rb, &cb_req))
    {
        if (cb_req.code == cmd)
        {
            if (size < 0)
            {
                LOG_W(TAG, "Got MSP error code %d, skipping callback", size);
                break;
            }
            if (cb_req.callback)
            {
                cb_req.callback(conn, cmd, data, size, cb_req.data);
            }
            break;
        }
        LOG_W(TAG, "Discaring callback for MSP code %d (%d in RB)", (int)cb_req.code, ring_buffer_count(&conn->rb));
    }
}

void msp_conn_set_global_callback(msp_conn_t *conn, msp_cmd_callback_f callback, void *data)
{
    conn->global_callback = callback;
    conn->global_callback_data = data;
}
