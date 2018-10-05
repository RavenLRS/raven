#pragma once

#include <stdint.h>

#include "rc/failsafe.h"
#include "rc/telemetry.h"

#include "util/data_state.h"
#include "util/time.h"

// This file contains the structures used for RC data.
// Inputs feed this module, which in turn feeds the outputs.

// We use the same channel values as CRSF. Minimim value is 172,
// maximum is 1811 and mid is 992. These gives us 1640 steps per
// channel which require 11 bits of precission. Additionaly, this
// lets us use do IO in CRSF, SBUS and Fport with minimal maths,
// since FrSky uses the same mapping, just one value bigger
// (i.e. min: 173, mid: 993 and max: 1812)
#define RC_CHANNEL_MIN_VALUE 172
#define RC_CHANNEL_CENTER_VALUE 992
#define RC_CHANNEL_MAX_VALUE 1811

#define RC_CHANNELS_NUM 16

#define RC_CHANNEL_ENCODE_TO_BITS(v, nbits) (((v - RC_CHANNEL_MIN_VALUE) * ((1 << nbits) - 1)) / (RC_CHANNEL_MAX_VALUE - RC_CHANNEL_MIN_VALUE))
// special case center value, might get distorted otherwise
#define RC_CHANNEL_DECODE_FROM_BITS(v, nbits) ({                                                                     \
    unsigned __dec;                                                                                                  \
    if (v == RC_CHANNEL_ENCODE_TO_BITS(RC_CHANNEL_CENTER_VALUE, nbits))                                              \
    {                                                                                                                \
        __dec = RC_CHANNEL_CENTER_VALUE;                                                                             \
    }                                                                                                                \
    else                                                                                                             \
    {                                                                                                                \
        __dec = (RC_CHANNEL_MIN_VALUE + ((v * (RC_CHANNEL_MAX_VALUE - RC_CHANNEL_MIN_VALUE)) / ((1 << nbits) - 1))); \
    }                                                                                                                \
    __dec;                                                                                                           \
})

#define RC_CHANNEL_VALUE_FROM_PERCENTAGE(p) (RC_CHANNEL_MIN_VALUE + ((RC_CHANNEL_MAX_VALUE - RC_CHANNEL_MIN_VALUE) * p) / 100)

typedef struct control_channel_s
{
    // Each channel has 1024 possible values. Value of 0
    // represents 988us, while 1023 represents 2012us.
    uint16_t value;
    data_state_t data_state;
} control_channel_t;

// We support RC_CHANNELS_NUM channels between radio<->TX and RX<->FC.
//
// Note, however, that the air protocol might not support
// all those channels with that same resolution. However,
// channels can be used for other purposes (e.g. injecting
// RX RSSI to the FC via a channel).

typedef struct rmp_s rmp_t;

typedef struct rc_data_s
{
    control_channel_t channels[RC_CHANNELS_NUM];
    // The number of channels in use. Might be lower than RC_CHANNELS_NUM
    // if the other end supports less channels, but never higher.
    unsigned channels_num;
    // Wether all the channels have valid values received from
    // the input.
    bool ready;
    // Pointers to failsafe states are stored in this struct
    // so outputs can have read access to the failsafe
    // state of the input.
    struct
    {
        const failsafe_t *input;
        const failsafe_t *output;
    } failsafe;
    telemetry_t telemetry_uplink[TELEMETRY_UPLINK_COUNT];
    telemetry_t telemetry_downlink[TELEMETRY_DOWNLINK_COUNT];
    // Provided here so inputs and outputs can both use
    // RMP messages.
    rmp_t *rmp;
} rc_data_t;

void rc_data_reset_input(rc_data_t *data);
void rc_data_reset_output(rc_data_t *data);

void rc_data_update_channel(rc_data_t *data, unsigned ch, unsigned value, time_micros_t now);
uint16_t rc_data_get_channel_value(const rc_data_t *data, unsigned ch);
bool rc_data_is_ready(rc_data_t *data);
bool rc_data_has_dirty_channels(rc_data_t *data);
// Used by the RX to keep track when the channels need to be flushed
// to the FC. ACK seq is not used, since differential updates only
// happen OTA.
void rc_data_channels_sent(rc_data_t *data, time_micros_t now);
unsigned rc_data_get_channel_percentage(const rc_data_t *data, unsigned ch);

telemetry_t *rc_data_get_telemetry(rc_data_t *data, int telemetry_id);

telemetry_t *rc_data_get_downlink_telemetry(rc_data_t *data, telemetry_downlink_id_e id);
telemetry_t *rc_data_get_uplink_telemetry(rc_data_t *data, telemetry_uplink_id_e id);

const char *rc_data_get_pilot_name(const rc_data_t *data);
const char *rc_data_get_craft_name(const rc_data_t *data);

bool rc_data_input_failsafe_is_active(const rc_data_t *data);
bool rc_data_output_failsafe_is_active(const rc_data_t *data);

#define TELEMETRY_GET_DOWNLINK_U8(rc_data, id) telemetry_get_u8(rc_data_get_downlink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_DOWNLINK_I8(rc_data, id) telemetry_get_i8(rc_data_get_downlink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_DOWNLINK_U16(rc_data, id) telemetry_get_u16(rc_data_get_downlink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_DOWNLINK_I16(rc_data, id) telemetry_get_i16(rc_data_get_downlink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_DOWNLINK_U32(rc_data, id) telemetry_get_u32(rc_data_get_downlink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_DOWNLINK_I32(rc_data, id) telemetry_get_i32(rc_data_get_downlink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_DOWNLINK_STR(rc_data, id) telemetry_get_str(rc_data_get_downlink_telemetry(rc_data, id), id)

#define TELEMETRY_GET_UPLINK_U8(rc_data, id) telemetry_get_u8(rc_data_get_uplink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_UPLINK_I8(rc_data, id) telemetry_get_i8(rc_data_get_uplink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_UPLINK_U16(rc_data, id) telemetry_get_u16(rc_data_get_uplink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_UPLINK_I16(rc_data, id) telemetry_get_i16(rc_data_get_uplink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_UPLINK_U32(rc_data, id) telemetry_get_u32(rc_data_get_uplink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_UPLINK_I32(rc_data, id) telemetry_get_i32(rc_data_get_uplink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_UPLINK_STR(rc_data, id) telemetry_get_str(rc_data_get_uplink_telemetry(rc_data, id), id)

#define TELEMETRY_GET_U8(rc_data, id) (TELEMETRY_IS_UPLINK(id) ? TELEMETRY_GET_UPLINK_U8(rc_data, id) : TELEMETRY_GET_DOWNLINK_U8(rc_data, id))
#define TELEMETRY_GET_I8(rc_data, id) (TELEMETRY_IS_UPLINK(id) ? TELEMETRY_GET_UPLINK_I8(rc_data, id) : TELEMETRY_GET_DOWNLINK_I8(rc_data, id))
#define TELEMETRY_GET_U16(rc_data, id) (TELEMETRY_IS_UPLINK(id) ? TELEMETRY_GET_UPLINK_U16(rc_data, id) : TELEMETRY_GET_DOWNLINK_U16(rc_data, id))
#define TELEMETRY_GET_I16(rc_data, id) (TELEMETRY_IS_UPLINK(id) ? TELEMETRY_GET_UPLINK_I16(rc_data, id) : TELEMETRY_GET_DOWNLINK_I16(rc_data, id))
#define TELEMETRY_GET_U32(rc_data, id) (TELEMETRY_IS_UPLINK(id) ? TELEMETRY_GET_UPLINK_U32(rc_data, id) : TELEMETRY_GET_DOWNLINK_U32(rc_data, id))
#define TELEMETRY_GET_I32(rc_data, id) (TELEMETRY_IS_UPLINK(id) ? TELEMETRY_GET_UPLINK_I32(rc_data, id) : TELEMETRY_GET_DOWNLINK_I32(rc_data, id))
#define TELEMETRY_GET_STR(rc_data, id) (TELEMETRY_IS_UPLINK(id) ? TELEMETRY_GET_UPLINK_STR(rc_data, id) : TELEMETRY_GET_DOWNLINK_STR(rc_data, id))

#define TELEMETRY_GET_UPLINK_U8(rc_data, id) telemetry_get_u8(rc_data_get_uplink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_UPLINK_I8(rc_data, id) telemetry_get_i8(rc_data_get_uplink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_UPLINK_U16(rc_data, id) telemetry_get_u16(rc_data_get_uplink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_UPLINK_I16(rc_data, id) telemetry_get_i16(rc_data_get_uplink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_UPLINK_U32(rc_data, id) telemetry_get_u32(rc_data_get_uplink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_UPLINK_I32(rc_data, id) telemetry_get_i32(rc_data_get_uplink_telemetry(rc_data, id), id)
#define TELEMETRY_GET_UPLINK_STR(rc_data, id) telemetry_get_str(rc_data_get_uplink_telemetry(rc_data, id), id)

#define TELEMETRY_SET_DOWNLINK_U8(rc_data, id, v, now) telemetry_set_u8(rc_data_get_downlink_telemetry(rc_data, id), id, v, now)
#define TELEMETRY_SET_DOWNLINK_I8(rc_data, id, v, now) telemetry_set_i8(rc_data_get_downlink_telemetry(rc_data, id), id, v, now)
#define TELEMETRY_SET_DOWNLINK_U16(rc_data, id, v, now) telemetry_set_u16(rc_data_get_downlink_telemetry(rc_data, id), id, v, now)
#define TELEMETRY_SET_DOWNLINK_I16(rc_data, id, v, now) telemetry_set_i16(rc_data_get_downlink_telemetry(rc_data, id), id, v, now)
#define TELEMETRY_SET_DOWNLINK_U32(rc_data, id, v, now) telemetry_set_u32(rc_data_get_downlink_telemetry(rc_data, id), id, v, now)
#define TELEMETRY_SET_DOWNLINK_I32(rc_data, id, v, now) telemetry_set_i32(rc_data_get_downlink_telemetry(rc_data, id), id, v, now)
#define TELEMETRY_SET_DOWNLINK_STR(rc_data, id, v, now) telemetry_set_str(rc_data_get_downlink_telemetry(rc_data, id), id, v, now)

#define TELEMETRY_SET_UPLINK_U8(rc_data, id, v, now) telemetry_set_u8(rc_data_get_uplink_telemetry(rc_data, id), id, v, now)
#define TELEMETRY_SET_UPLINK_I8(rc_data, id, v, now) telemetry_set_i8(rc_data_get_uplink_telemetry(rc_data, id), id, v, now)
#define TELEMETRY_SET_UPLINK_U16(rc_data, id, v, now) telemetry_set_u16(rc_data_get_uplink_telemetry(rc_data, id), id, v, now)
#define TELEMETRY_SET_UPLINK_I16(rc_data, id, v, now) telemetry_set_i16(rc_data_get_uplink_telemetry(rc_data, id), id, v, now)
#define TELEMETRY_SET_UPLINK_U32(rc_data, id, v, now) telemetry_set_u32(rc_data_get_uplink_telemetry(rc_data, id), id, v, now)
#define TELEMETRY_SET_UPLINK_I32(rc_data, id, v, now) telemetry_set_i32(rc_data_get_uplink_telemetry(rc_data, id), id, v, now)
#define TELEMETRY_SET_UPLINK_STR(rc_data, id, v, now) telemetry_set_str(rc_data_get_uplink_telemetry(rc_data, id), id, v, now)

#define TELEMETRY_SET_U8(rc_data, id, v, now) (TELEMETRY_IS_UPLINK(id) ? TELEMETRY_SET_UPLINK_U8(rc_data, id, v, now) : TELEMETRY_SET_DOWNLINK_U8(rc_data, id, v, now))
#define TELEMETRY_SET_I8(rc_data, id, v, now) (TELEMETRY_IS_UPLINK(id) ? TELEMETRY_SET_UPLINK_I8(rc_data, id, v, now) : TELEMETRY_SET_DOWNLINK_I8(rc_data, id, v, now))
#define TELEMETRY_SET_U16(rc_data, id, v, now) (TELEMETRY_IS_UPLINK(id) ? TELEMETRY_SET_UPLINK_U16(rc_data, id, v, now) : TELEMETRY_SET_DOWNLINK_U16(rc_data, id, v, now))
#define TELEMETRY_SET_I16(rc_data, id, v, now) (TELEMETRY_IS_UPLINK(id) ? TELEMETRY_SET_UPLINK_I16(rc_data, id, v, now) : TELEMETRY_SET_DOWNLINK_I16(rc_data, id, v, now))
#define TELEMETRY_SET_U32(rc_data, id, v, now) (TELEMETRY_IS_UPLINK(id) ? TELEMETRY_SET_UPLINK_U32(rc_data, id, v, now) : TELEMETRY_SET_DOWNLINK_U32(rc_data, id, v, now))
#define TELEMETRY_SET_I32(rc_data, id, v, now) (TELEMETRY_IS_UPLINK(id) ? TELEMETRY_SET_UPLINK_I32(rc_data, id, v, now) : TELEMETRY_SET_DOWNLINK_I32(rc_data, id, v, now))
#define TELEMETRY_SET_STR(rc_data, id, v, now) (TELEMETRY_IS_UPLINK(id) ? TELEMETRY_SET_UPLINK_STR(rc_data, id, v, now) : TELEMETRY_SET_DOWNLINK_STR(rc_data, id, v, now))