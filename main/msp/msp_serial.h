#pragma once

#include <stddef.h>
#include <stdint.h>

#include "io/io.h"

#include "msp/msp_transport.h"

typedef enum {
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
} msp_serial_t;

void msp_serial_init(msp_serial_t *tr, io_t *io);