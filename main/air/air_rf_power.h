#pragma once

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
int air_rf_power_to_dbm(air_rf_power_e power);