#pragma once

#include <stdint.h>

typedef enum
{
    AIR_BAND_147 = 1,
    AIR_BAND_169,
    AIR_BAND_315,
    AIR_BAND_433,
    AIR_BAND_470,
    AIR_BAND_868,
    AIR_BAND_915,
} air_band_e;

#define AIR_BAND_INVALID 0
#define AIR_BAND_MIN AIR_BAND_147
#define AIR_BAND_MAX AIR_BAND_915
#define AIR_BAND_BIT(band) (1 << band)

typedef uint16_t air_band_mask_t;

unsigned long air_band_frequency(air_band_e band);
air_band_e air_band_mask_get_band(air_band_mask_t mask, int index);
