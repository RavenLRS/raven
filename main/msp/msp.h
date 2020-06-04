#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "util/ringbuffer.h"

// MSP codes we use
#define MSP_FC_VARIANT 2
#define MSP_FC_VERSION 3
#define MSP_NAME 10
#define MSP_CURRENT_METER_CONFIG 40
#define MSP_RSSI_CONFIG 50
#define MSP_RAW_IMU 102
#define MSP_RAW_GPS 106
#define MSP_ATTITUDE 108
#define MSP_ALTITUDE 109
#define MSP_ANALOG 110
#define MSP_MISC 114
#define MSP_SET_TX_INFO 186
#define MSP_SET_RAW_RC 200

// This is the maximum payload size we accept. MSP doesn't have
// an upper boundary on payload sizes.
#define MSP_MAX_PAYLOAD_SIZE 512

typedef enum
{
    MSP_DIRECTION_TO_MWC,
    MSP_DIRECTION_FROM_MWC,
    MSP_DIRECTION_ERROR, // Set by the FC as an error response
} msp_direction_e;

enum
{
    MSP_EOF = -1,
    MSP_INVALID_CHECKSUM = -2,
    MSP_BUF_TOO_SMALL = -3,
    MSP_BUSY = -4,
};

typedef struct msp_conn_s msp_conn_t;

typedef void (*msp_cmd_callback_f)(msp_conn_t *conn, uint16_t cmd, const void *payload, int size, void *callback_data);

typedef struct msp_callback_req_s
{
    uint16_t code;
    msp_cmd_callback_f callback;
    void *data;
} msp_callback_req_t;

#define MSP_QUEUE_MAX_SIZE 10

typedef struct msp_transport_s msp_transport_t;

typedef struct msp_conn_s
{
    RING_BUFFER_DECLARE(rb, msp_callback_req_t, MSP_QUEUE_MAX_SIZE);
    msp_transport_t *transport;
    msp_cmd_callback_f global_callback;
    void *global_callback_data;
} msp_conn_t;

void msp_conn_init(msp_conn_t *conn, msp_transport_t *transport);
void msp_conn_update(msp_conn_t *conn);
int msp_conn_write(msp_conn_t *conn, msp_direction_e direction, uint16_t cmd, const void *payload, size_t size);
int msp_conn_send(msp_conn_t *conn, uint16_t cmd, const void *payload, size_t size, msp_cmd_callback_f callback, void *callback_data);

// Used by transports that push data rather than being polled
void msp_conn_dispatch_message(msp_conn_t *conn, msp_direction_e direction, uint16_t cmd, const void *data, int size);

// Sets a global callback for all the decoded messages. The callback in msp_conn_write() is ignored while
// this is non-NULL. Set to NULL to clear.
void msp_conn_set_global_callback(msp_conn_t *conn, msp_cmd_callback_f callback, void *data);