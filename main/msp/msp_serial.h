#pragma once

#include <stddef.h>
#include <stdint.h>

#include "io/io.h"

#include "msp/msp_transport.h"

#include "util/time.h"

// Required space for the protocol in addition to the data we want to send
#define MSP_V1_PROTOCOL_BYTES 6
#define MSP_V2_PROTOCOL_BYTES 9

#define MSP_SERIAL_DIRECTION_TO_MWC_BYTE '<'
#define MSP_SERIAL_DIRECTION_FROM_MWC_BYTE '>'
#define MSP_SERIAL_DIRECTION_ERROR_BYTE '!'

typedef enum
{
    MSP_SERIAL_BAUDRATE_115200 = 0,

    MSP_SERIAL_BAUDRATE_COUNT,
    MSP_SERIAL_BAUDRATE_FIRST = MSP_SERIAL_BAUDRATE_115200,
    MSP_SERIAL_BAUDRATE_LAST = MSP_SERIAL_BAUDRATE_115200,
} msp_serial_baudrate_e;

// Returns the baud rate for given MSP baud rate constant.
int msp_serial_baudrate_get(msp_serial_baudrate_e br);

typedef struct msp_serial_s
{
    msp_transport_t transport;
    uint8_t buf[MSP_MAX_PAYLOAD_SIZE];
    int buf_pos;
    io_t io;
    struct
    {
        bool active;
        uint32_t bytes_per_second;
        time_micros_t last_write;
        size_t last_write_size;
        time_micros_t response_pending_until;
        bool response_pending_is_estimate;
        size_t expected_response_size;
    } half_duplex;
} msp_serial_t;

void msp_serial_init(msp_serial_t *tr, io_t *io);