#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "io/io.h"

#define IBUS_BAUDRATE 115200
#define IBUS_NUM_CHANNELS 14
#define IBUS_FRAME_SIZE_MAX 16 * 2 + 2 + 2
#define IBUS_CHANNEL_VALUE_MIN 1000
#define IBUS_CHANNEL_VALUE_MID 1500
#define IBUS_CHANNEL_VALUE_MAX 2000

typedef struct rc_data_s rc_data_t;

typedef enum
{
    IBUS_FRAMETYPE_RC_CHANNELS = 0x40,
} input_frame_type_e;

typedef struct ibus_payload_s
{
    uint8_t pack_len : 8;
    input_frame_type_e pack_type : 8;
    uint16_t ch[IBUS_NUM_CHANNELS];
    uint16_t checksum;
} __attribute__((packed)) ibus_payload_t;

typedef union {
    uint8_t bytes[sizeof(ibus_payload_t)];
    ibus_payload_t payload;
} ibus_frame_t;

typedef void (*ibus_frame_f)(void *data, ibus_frame_t *frame);

typedef struct ibus_port_s
{
    io_t io;
    ibus_frame_f frame_callback;
    void *callback_data;
    uint8_t buf[IBUS_FRAME_SIZE_MAX];
    unsigned buf_pos;
} ibus_port_t;

// IBUS uses the same channel stepping and numbering as our internal representation,
// just with +1 offset.
inline unsigned channel_from_ibus_value(unsigned ibus_val) { return ibus_val - 1; }
inline unsigned channel_to_ibus_value(unsigned val) { return val + 1; }

void ibus_port_init(ibus_port_t *port, io_t *io, ibus_frame_f frame_callback, void *callback_data);

bool ibus_port_decode(ibus_port_t *port);
void ibus_port_reset(ibus_port_t *port);
