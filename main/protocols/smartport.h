#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "io/io.h"

#include "msp/msp_telemetry.h"

#include "rc/telemetry.h"

#include "util/macros.h"
#include "util/ringbuffer.h"
#include "util/time.h"

#define SMARTPORT_BAUDRATE 57600
#define SMARTPORT_START_STOP 0x7E
#define SMARTPORT_BYTE_STUFF 0x7D
#define SMARTPORT_XOR 0x20

#define SMARTPORT_SENSOR_ID_COUNT 28

typedef struct smartport_payload_s
{
    uint8_t frame_id;
    uint16_t value_id;
    uint32_t data;
} PACKED smartport_payload_t;

typedef enum
{
    SMARTPORT_PAYLOAD_FRAME_STATE_INCOMPLETE,
    SMARTPORT_PAYLOAD_FRAME_STATE_BYTESTUFF,
    SMARTPORT_PAYLOAD_FRAME_STATE_CHECKSUM,
    SMARTPORT_PAYLOAD_FRAME_STATE_INVALID,
    SMARTPORT_PAYLOAD_FRAME_STATE_COMPLETE,
} smartport_payload_frame_state_e;

typedef struct
{
    union {
        smartport_payload_t payload;
        uint8_t bytes[sizeof(smartport_payload_t)];
    };
    uint8_t pos;
    smartport_payload_frame_state_e state;
} smartport_payload_frame_t;

typedef struct smartport_master_s
{
    time_ticks_t next_poll;
    uint8_t last_polled;
    uint8_t last_found_polled;
    bool found[SMARTPORT_SENSOR_ID_COUNT];
    uint8_t found_count;
    bool last_poll_from_found;
    io_t io;
    telemetry_downlink_val_f telemetry_found;
    void *telemetry_data;

    msp_telemetry_t msp_telemetry;

    // Used for temporary storage
    smartport_payload_frame_t frame;
} smartport_master_t;

void smartport_master_init(smartport_master_t *sp, io_t *io);
void smartport_master_update(smartport_master_t *sp);
smartport_payload_t *smartport_master_get_last_payload(smartport_master_t *sp);

// Used by FPort
bool smartport_master_decode_payload(smartport_master_t *sp, const smartport_payload_t *payload);
