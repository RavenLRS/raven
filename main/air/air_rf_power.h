#pragma once

#include "util/macros.h"

typedef enum
{
    AIR_RF_POWER_AUTO = 0,
    AIR_RF_POWER_1mw,
    AIR_RF_POWER_10mw,
    AIR_RF_POWER_25mw,
    AIR_RF_POWER_50mw,
    AIR_RF_POWER_100mw,

    AIR_RF_POWER_FIRST = AIR_RF_POWER_AUTO,
    AIR_RF_POWER_LAST = AIR_RF_POWER_100mw,
    AIR_RF_POWER_DEFAULT = AIR_RF_POWER_AUTO,
} air_rf_power_e;

// returns power in dBm
inline int air_rf_power_to_dbm(air_rf_power_e power)
{
    switch (power)
    {
    case AIR_RF_POWER_1mw:
        return 0;
    case AIR_RF_POWER_10mw:
        return 10;
    case AIR_RF_POWER_25mw:
        return 14;
    case AIR_RF_POWER_AUTO:
        // TODO: Actual dynamic power. Uses fixed 50mw for now.
    case AIR_RF_POWER_50mw:
        return 17;
    case AIR_RF_POWER_100mw:
        return 20;
    }
    UNREACHABLE();
    return 0;
}