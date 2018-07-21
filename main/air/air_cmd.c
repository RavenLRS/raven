#include <limits.h>

#include "air_cmd.h"

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
