#include "air/air_rf_power.h"

#include "util/macros.h"

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
