#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "msp/msp_transport.h"

#include "util/ringbuffer.h"
#include "util/time.h"

#define MSP_TELEMETRY_REQ_QUEUE_SIZE 512
#define MSP_TELEMETRY_RESP_QUEUE_SIZE 512

// A telemetry transport implementation which splits MSP writes into chunks
// up to maximum a given size. Used for sending MSP requests over S.Port
// and CRSF. Note that this transport has two possible modes of operation,
// depending on if it's used for input or output because MSP over telemetry
// uses different representations for requests and responses.
typedef struct msp_telemetry_s
{
    msp_transport_t transport;

    // We can only send one command at a time.
    RING_BUFFER_DECLARE(req, uint8_t, MSP_TELEMETRY_REQ_QUEUE_SIZE);
    RING_BUFFER_DECLARE(resp, uint8_t, MSP_TELEMETRY_RESP_QUEUE_SIZE);
    unsigned count;            // number of full requests (input) or responses (output) in the buffer
    size_t max_size;           // max req (input) or resp (output) chunk size asked by the caller
    uint8_t cmd;               // the cmd being handled. note that 0 is a valid MSP cmd, so we need a flag
    time_ticks_t in_use_since; // wether a command is in flight
    unsigned req_seq : 4;      // sequence numbers for requests
    unsigned resp_seq : 4;     // sequence numbers for responses
    uint8_t size;              // expected size of the MSP request or response - required because of padding
    uint8_t recv;              // number of bytes already received from the request or response
} msp_telemetry_t;

// INPUT mode
void msp_telemetry_init_input(msp_telemetry_t *tr, size_t max_resp_chunk_size);

// Called to feed MSP over telemetry request data
bool msp_telemetry_push_request_chunk(msp_telemetry_t *tr, const void *payload, size_t size);
// Retrieve MSP over telemetry request response to send
size_t msp_telemetry_pop_response_chunk(msp_telemetry_t *tr, void *buf);

// OUTPUT mode
void msp_telemetry_init_output(msp_telemetry_t *tr, size_t max_req_chunk_size);

// Called to feed MSP over telemetry response data
bool msp_telemetry_push_response_chunk(msp_telemetry_t *tr, const void *payload, size_t size);
// Retrieve MSP over telemetry request data to send
size_t msp_telemetry_pop_request_chunk(msp_telemetry_t *tr, void *buf);
