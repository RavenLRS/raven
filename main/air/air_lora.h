#pragma once

#include <stdint.h>

#include "platform.h"

#include "util/macros.h"
#include "util/time.h"

#define AIR_LORA_MODE_BIT(mode) (1 << mode)
#define AIR_LORA_BAND_BIT(band) (1 << band)

typedef struct lora_s lora_t;

typedef enum {
    AIR_LORA_MODE_1 = 1, // SF6, BW500 - 10ms cycle
    AIR_LORA_MODE_2,     // SF7, BW500 - 17ms cycle
    AIR_LORA_MODE_3,     // SF8, BW500 - 31ms cycle
    AIR_LORA_MODE_4,     // SF9, BW500 - 55ms cycle
    AIR_LORA_MODE_5,     // SF10, BW500 - 110ms cycle
    AIR_LORA_MODE_FASTEST = AIR_LORA_MODE_1,
    AIR_LORA_MODE_LONGEST = AIR_LORA_MODE_5,
} air_lora_mode_e;

#define AIR_LORA_MODE_INVALID 0

typedef uint16_t air_lora_mode_mask_t;

typedef enum {
    AIR_LORA_SUPPORTED_MODES_FIXED_1 = 1,
    AIR_LORA_SUPPORTED_MODES_FIXED_2 = 2,
    AIR_LORA_SUPPORTED_MODES_FIXED_3 = 3,
    AIR_LORA_SUPPORTED_MODES_FIXED_4 = 4,
    AIR_LORA_SUPPORTED_MODES_FIXED_5 = 5,
    AIR_LORA_SUPPORTED_MODES_1_TO_5 = 32,
    AIR_LORA_SUPPORTED_MODES_2_TO_5 = 33,
} air_lora_supported_modes_e;

typedef enum {
    AIR_LORA_BAND_147 = 1,
    AIR_LORA_BAND_169,
    AIR_LORA_BAND_315,
    AIR_LORA_BAND_433,
    AIR_LORA_BAND_470,
    AIR_LORA_BAND_868,
    AIR_LORA_BAND_915,
} air_lora_band_e;

#define AIR_LORA_BAND_INVALID 0
#define AIR_LORA_BAND_MIN AIR_LORA_BAND_147
#define AIR_LORA_BAND_MAX AIR_LORA_BAND_915

typedef uint16_t air_lora_band_mask_t;

typedef struct air_lora_config_s
{
    lora_t *lora;
    air_lora_band_e band;
    air_lora_supported_modes_e modes;
    air_lora_band_mask_t bands;
} air_lora_config_t;

#define AIR_LORA_MODE_COUNT (AIR_LORA_MODE_LONGEST - AIR_LORA_MODE_FASTEST + 1)

inline bool air_lora_mode_is_valid(air_lora_mode_e mode)
{
    return mode >= AIR_LORA_MODE_FASTEST && mode <= AIR_LORA_MODE_LONGEST;
}

inline bool air_lora_mode_mask_contains(air_lora_mode_mask_t mask, air_lora_mode_e mode)
{
    return mask & AIR_LORA_MODE_BIT(mode);
}

inline air_lora_mode_mask_t air_lora_mode_mask_remove(air_lora_mode_mask_t mask, air_lora_mode_e mode)
{
    return mask & ~AIR_LORA_MODE_BIT(mode);
}

inline air_lora_mode_e _air_lora_mode_faster(air_lora_mode_e mode)
{
    if (mode > AIR_LORA_MODE_FASTEST)
    {
        return mode - 1;
    }
    return AIR_LORA_MODE_INVALID;
}

inline air_lora_mode_e _air_lora_mode_longer(air_lora_mode_e mode)
{
    if (mode < AIR_LORA_MODE_LONGEST)
    {
        return mode + 1;
    }
    return AIR_LORA_MODE_INVALID;
}

inline air_lora_mode_e air_lora_mode_faster(air_lora_mode_e mode, air_lora_mode_mask_t supported)
{
    while (1)
    {
        air_lora_mode_e faster = _air_lora_mode_faster(mode);
        if (!air_lora_mode_is_valid(faster))
        {
            // No faster modes left
            break;
        }
        if (air_lora_mode_mask_contains(supported, faster))
        {
            return faster;
        }
        mode = faster;
    }
    return AIR_LORA_MODE_INVALID;
}

inline air_lora_mode_e air_lora_mode_longer(air_lora_mode_e mode, air_lora_mode_mask_t supported)
{
    while (1)
    {
        air_lora_mode_e longer = _air_lora_mode_longer(mode);
        if (!air_lora_mode_is_valid(longer))
        {
            // No faster modes left
            break;
        }
        if (air_lora_mode_mask_contains(supported, longer))
        {
            return longer;
        }
        mode = longer;
    }
    return AIR_LORA_MODE_INVALID;
}

inline air_lora_mode_e air_lora_mode_fastest(air_lora_mode_mask_t supported)
{
    for (int ii = AIR_LORA_MODE_FASTEST; ii <= AIR_LORA_MODE_LONGEST; ii++)
    {
        if (air_lora_mode_mask_contains(supported, ii))
        {
            return ii;
        }
    }
    return AIR_LORA_MODE_INVALID;
}

inline air_lora_mode_e air_lora_mode_longest(air_lora_mode_mask_t supported)
{
    for (int ii = AIR_LORA_MODE_LONGEST; ii >= AIR_LORA_MODE_FASTEST; ii--)
    {
        if (air_lora_mode_mask_contains(supported, ii))
        {
            return ii;
        }
    }
    return AIR_LORA_MODE_INVALID;
}

inline air_lora_mode_mask_t air_lora_modes_pack(air_lora_supported_modes_e supported)
{
    air_lora_mode_mask_t modes = 0;
    switch (supported)
    {
    case AIR_LORA_SUPPORTED_MODES_FIXED_1:
        modes |= AIR_LORA_MODE_BIT(AIR_LORA_MODE_1);
        break;
    case AIR_LORA_SUPPORTED_MODES_FIXED_2:
        modes |= AIR_LORA_MODE_BIT(AIR_LORA_MODE_2);
        break;
    case AIR_LORA_SUPPORTED_MODES_FIXED_3:
        modes |= AIR_LORA_MODE_BIT(AIR_LORA_MODE_3);
        break;
    case AIR_LORA_SUPPORTED_MODES_FIXED_4:
        modes |= AIR_LORA_MODE_BIT(AIR_LORA_MODE_4);
        break;
    case AIR_LORA_SUPPORTED_MODES_FIXED_5:
        modes |= AIR_LORA_MODE_BIT(AIR_LORA_MODE_5);
        break;
    case AIR_LORA_SUPPORTED_MODES_1_TO_5:
        for (int ii = AIR_LORA_MODE_1; ii <= AIR_LORA_MODE_5; ii++)
        {
            modes |= AIR_LORA_MODE_BIT(ii);
        }
        break;
    case AIR_LORA_SUPPORTED_MODES_2_TO_5:
        for (int ii = AIR_LORA_MODE_2; ii <= AIR_LORA_MODE_5; ii++)
        {
            modes |= AIR_LORA_MODE_BIT(ii);
        }
        break;
    }
    return modes;
}

inline bool air_lora_modes_intersect(air_lora_mode_mask_t *intersection,
                                     air_lora_supported_modes_e s1,
                                     air_lora_supported_modes_e s2)
{
    air_lora_mode_mask_t i = air_lora_modes_pack(s1) & air_lora_modes_pack(s2);
    if (intersection)
    {
        *intersection = i;
    }
    return i != 0;
}

inline unsigned long air_lora_band_frequency(air_lora_band_e band)
{
#define MHZ(n) (n * 1000000)
    switch (band)
    {
    case AIR_LORA_BAND_147:
        return MHZ(147);
    case AIR_LORA_BAND_169:
        return MHZ(169);
    case AIR_LORA_BAND_315:
        return MHZ(315);
    case AIR_LORA_BAND_433:
        return MHZ(433);
    case AIR_LORA_BAND_470:
        return MHZ(470);
    case AIR_LORA_BAND_868:
        return MHZ(868);
    case AIR_LORA_BAND_915:
        return MHZ(915);
    }
#undef MHZ
    UNREACHABLE();
    return 0;
}

void air_lora_set_parameters(lora_t *lora, air_lora_mode_e mode);
time_micros_t air_lora_full_cycle_time(air_lora_mode_e mode);
time_micros_t air_lora_uplink_cycle_time(air_lora_mode_e mode);
bool air_lora_cycle_is_full(air_lora_mode_e mode, unsigned seq);
time_micros_t air_lora_tx_failsafe_interval(air_lora_mode_e mode);
time_micros_t air_lora_rx_failsafe_interval(air_lora_mode_e mode);

void air_lora_set_parameters_bind(lora_t *lora);

air_lora_band_e air_lora_band_mask_get_band(air_lora_band_mask_t mask, int index);
