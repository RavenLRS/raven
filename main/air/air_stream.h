#pragma once

#include <stdint.h>

#include "air/air.h"
#include "air/air_cmd.h"

#include "msp/msp.h"

#include "rc/telemetry.h"

#include "util/ringbuffer.h"
#include "util/time.h"

// MSP_MAX_PAYLOAD_SIZE, 1 byte for direction, 3 bytes for variable sized cmd and
// 3 bytes for the variable sized cmd size
#define AIR_STREAM_BUFFER_CAPACITY (MSP_MAX_PAYLOAD_SIZE + 1 + 3 + 3)
// Worst case scenario: All bytes stuffed since we don't put start-stop here
#define AIR_STREAM_INPUT_BUFFER_CAPACITY (AIR_STREAM_BUFFER_CAPACITY * 2)
// Worst case scenario: All bytes stuffed plus start-stop starting with just one byte left in the packet
#define AIR_STREAM_OUTPUT_BUFFER_CAPACITY (AIR_STREAM_BUFFER_CAPACITY * 2 + 1 + 1)
#define AIR_STREAM_MAX_PAYLOAD_SIZE AIR_STREAM_BUFFER_CAPACITY

// Value is already converted to rc_data_t units
typedef void (*air_stream_channel_f)(void *user, unsigned chn, unsigned value, time_micros_t now);
typedef void (*air_stream_telemetry_f)(void *user, int telemetry_id, const void *data, size_t size, time_micros_t now);
typedef void (*air_stream_cmd_f)(void *user, air_cmd_e cmd_id, const void *data, size_t size, time_micros_t now);

typedef struct air_stream_s
{
    air_stream_channel_f channel;
    air_stream_telemetry_f telemetry;
    air_stream_cmd_f cmd;
    void *user;
    bool input_in_sync;                // Wether the input data stream is synchronized
    unsigned input_seq : AIR_SEQ_BITS; // Input sequence number
    RING_BUFFER_DECLARE(input_buf, uint8_t, AIR_STREAM_INPUT_BUFFER_CAPACITY);
    RING_BUFFER_DECLARE(output_buf, uint8_t, AIR_STREAM_OUTPUT_BUFFER_CAPACITY);
} air_stream_t;

void air_stream_init(air_stream_t *s, air_stream_channel_f channel, air_stream_telemetry_f telemetry, air_stream_cmd_f cmd, void *user);

// Add data received from the air. Returns true iff the data didn't cause a reset
// in the stream.
void air_stream_feed_input(air_stream_t *s, unsigned seq, const void *data, size_t size, time_micros_t now);

// Add data to be stream to the air
size_t air_stream_feed_output_channel(air_stream_t *s, unsigned ch, unsigned val);
size_t air_stream_feed_output_uplink_telemetry(air_stream_t *s, telemetry_t *t, telemetry_uplink_id_e id);
size_t air_stream_feed_output_downlink_telemetry(air_stream_t *s, telemetry_t *t, telemetry_downlink_id_e id);
size_t air_stream_feed_output_cmd(air_stream_t *s, uint8_t cmd, const void *data, size_t size);
// Returns number of bytes ready for output
size_t air_stream_output_count(const air_stream_t *s);
// Removes all output data from the air stream. Used for
// sending urgent data.
void air_stream_reset_output(air_stream_t *s);
bool air_stream_pop_output(air_stream_t *s, uint8_t *c);
