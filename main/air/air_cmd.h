#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "air/air.h"
#include "air/air_mode.h"

#include "util/macros.h"

// air_cmd_e max possible value is 63, see air_stream_feed_output_cmd
typedef enum
{
    AIR_CMD_SWITCH_MODE_ACK = 0,
    AIR_CMD_SWITCH_MODE_1 = 1,
    AIR_CMD_SWITCH_MODE_2 = 2,
    AIR_CMD_SWITCH_MODE_3 = 3,
    AIR_CMD_SWITCH_MODE_4 = 4,
    AIR_CMD_SWITCH_MODE_5 = 5,

    AIR_CMD_REJECT_MODE = 31,
    AIR_CMD_MSP = 32,
    AIR_CMD_RMP = 33,
} air_cmd_e;

inline air_mode_e air_mode_from_cmd(air_cmd_e cmd)
{
    _Static_assert(AIR_CMD_SWITCH_MODE_1 == (air_cmd_e)AIR_MODE_1, "AIR_CMD_SWITCH_MODE_1 != AIR_MODE_1");
    _Static_assert(AIR_CMD_SWITCH_MODE_2 == (air_cmd_e)AIR_MODE_2, "AIR_CMD_SWITCH_MODE_2 != AIR_MODE_2");
    _Static_assert(AIR_CMD_SWITCH_MODE_3 == (air_cmd_e)AIR_MODE_3, "AIR_CMD_SWITCH_MODE_3 != AIR_MODE_3");
    _Static_assert(AIR_CMD_SWITCH_MODE_4 == (air_cmd_e)AIR_MODE_4, "AIR_CMD_SWITCH_MODE_4 != AIR_MODE_4");
    _Static_assert(AIR_CMD_SWITCH_MODE_5 == (air_cmd_e)AIR_MODE_5, "AIR_CMD_SWITCH_MODE_5 != AIR_MODE_5");

    // XXX: This assumes modes have the same number as the commands
    return (air_mode_e)cmd;
}

inline air_cmd_e air_cmd_switch_mode_from_mode(air_mode_e mode)
{
    // XXX: Same as air_mode_from_cmd
    return (air_cmd_e)mode;
}

typedef struct air_cmd_switch_mode_ack_s
{
    air_mode_e mode : 4;
    // The change will be performed BEFORE transmitting
    // this TX seq.
    unsigned at_tx_seq : AIR_SEQ_BITS;
} PACKED air_cmd_switch_mode_ack_t;

_Static_assert(sizeof(air_cmd_switch_mode_ack_t) == 1, "invalid air_cmd_switch_mode_ack_t size");

bool air_cmd_switch_mode_ack_in_progress(air_cmd_switch_mode_ack_t *cmd);
void air_cmd_switch_mode_ack_reset(air_cmd_switch_mode_ack_t *cmd);
// Returns true iff the switch should be now performed
bool air_cmd_switch_mode_ack_proceed(air_cmd_switch_mode_ack_t *cmd, unsigned tx_seq);
void air_cmd_switch_mode_ack_copy(air_cmd_switch_mode_ack_t *dst, const air_cmd_switch_mode_ack_t *src);
// Command payload size. <0 means explicit length using variable length
// encoding.
int air_cmd_size(air_cmd_e cmd);