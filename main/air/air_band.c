#include "air/air_band.h"

#include "util/macros.h"

unsigned long air_band_frequency(air_band_e band)
{
#define MHZ(n) (n * 1000000)
    switch (band)
    {
    case AIR_BAND_147:
        return MHZ(147);
    case AIR_BAND_169:
        return MHZ(169);
    case AIR_BAND_315:
        return MHZ(315);
    case AIR_BAND_433:
        return MHZ(433);
    case AIR_BAND_470:
        return MHZ(470);
    case AIR_BAND_868:
        return MHZ(868);
    case AIR_BAND_915:
        return MHZ(915);
    }
#undef MHZ
    UNREACHABLE();
    return 0;
}

air_band_e air_band_mask_get_band(air_band_mask_t mask, int index)
{
    int count = 0;
    for (int ii = AIR_BAND_MIN; ii <= AIR_BAND_MAX; ii++)
    {
        if (mask & AIR_BAND_BIT(ii))
        {
            if (count == index)
            {
                return ii;
            }
            count++;
        }
    }
    return AIR_BAND_INVALID;
}
