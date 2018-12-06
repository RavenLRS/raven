#include <limits.h>

#include "air_cmd.h"

bool air_cmd_switch_mode_ack_in_progress(air_cmd_switch_mode_ack_t *cmd)
{
    return cmd->mode != 0;
}

void air_cmd_switch_mode_ack_reset(air_cmd_switch_mode_ack_t *cmd)
{
    *((uint8_t *)cmd) = 0;
}

bool air_cmd_switch_mode_ack_proceed(air_cmd_switch_mode_ack_t *cmd, unsigned tx_seq)
{
    return air_cmd_switch_mode_ack_in_progress(cmd) && cmd->at_tx_seq == tx_seq;
}

void air_cmd_switch_mode_ack_copy(air_cmd_switch_mode_ack_t *dst, const air_cmd_switch_mode_ack_t *src)
{
    *((uint8_t *)dst) = *((uint8_t *)src);
}

int air_cmd_size(air_cmd_e cmd)
{
    switch (cmd)
    {
    case AIR_CMD_SWITCH_MODE_ACK:
        return 1;
    case AIR_CMD_SWITCH_MODE_1:
    case AIR_CMD_SWITCH_MODE_2:
    case AIR_CMD_SWITCH_MODE_3:
    case AIR_CMD_SWITCH_MODE_4:
    case AIR_CMD_SWITCH_MODE_5:
        return 0;
    case AIR_CMD_REJECT_MODE:
        return 1;
    case AIR_CMD_MSP:
    case AIR_CMD_RMP:
        return -1;
    }
    return INT_MAX;
}
