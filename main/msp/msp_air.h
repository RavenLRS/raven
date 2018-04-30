#pragma once

#include <stddef.h>

#include "msp/msp_transport.h"

typedef struct air_stream_s air_stream_t;
typedef struct msp_air_s msp_air_t;

typedef void (*msp_air_before_feed_f)(msp_air_t *air, size_t size, void *user_data);

typedef struct msp_air_s
{
    msp_transport_t transport;
    air_stream_t *air_stream;
    msp_air_before_feed_f before_feed;
    void *user_data;
} msp_air_t;

void msp_air_init(msp_air_t *tr, air_stream_t *stream, msp_air_before_feed_f before_feed, void *user_data);
void msp_air_dispatch(msp_air_t *tr, msp_conn_t *conn, const void *payload, size_t size);