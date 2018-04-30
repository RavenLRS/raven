#pragma once

#include "platform.h"

#include "util/macros.h"
#include "util/time.h"

typedef struct lora_s lora_t;

typedef enum {
    AIR_LORA_MODE_1 = 1,                     // SF6, BW500 - 10ms cycle
    AIR_LORA_MODE_2,                         // SF7, BW500 - 17ms cycle
    AIR_LORA_MODE_3,                         // SF8, BW500 - 31ms cycle
    AIR_LORA_MODE_4,                         // SF9, BW500 - 55ms cycle
    AIR_LORA_MODE_5,                         // SF10, BW500 - 110ms cycle
    AIR_LORA_MODE_FASTEST = AIR_LORA_MODE_2, // TODO
    AIR_LORA_MODE_LONGEST = AIR_LORA_MODE_5,
} air_lora_mode_e;

typedef enum {
#if defined(LORA_BAND_433)
    AIR_LORA_BAND_433,
#endif
#if defined(LORA_BAND_868)
    AIR_LORA_BAND_868,
#endif
#if defined(LORA_BAND_915)
    AIR_LORA_BAND_915,
#endif
    AIR_LORA_BAND_DEFAULT = 0,
} air_lora_band_e;

#define AIR_LORA_MODE_COUNT (AIR_LORA_MODE_LONGEST - AIR_LORA_MODE_FASTEST + 1)

inline air_lora_mode_e air_lora_mode_faster(air_lora_mode_e mode)
{
    if (mode > AIR_LORA_MODE_FASTEST)
    {
        return mode - 1;
    }
    return mode;
}

inline air_lora_mode_e air_lora_mode_longer(air_lora_mode_e mode)
{
    if (mode < AIR_LORA_MODE_LONGEST)
    {
        return mode + 1;
    }
    return mode;
}

inline uint8_t air_lora_modes_pack(void)
{
    uint8_t modes = 0;
    for (int ii = AIR_LORA_MODE_FASTEST; ii <= AIR_LORA_MODE_LONGEST; ii++)
    {
        modes |= (1 << ii);
    }
    return modes;
}

inline bool air_lora_modes_unpack(uint8_t modes, air_lora_mode_e *fastest, air_lora_mode_e *longest)
{
    bool fastest_ok = false;
    bool longest_ok = false;
    for (int ii = AIR_LORA_MODE_FASTEST; ii <= AIR_LORA_MODE_LONGEST; ii++)
    {
        if (modes & (1 << ii))
        {
            *fastest = ii;
            fastest_ok = true;
            break;
        }
    }
    for (int ii = AIR_LORA_MODE_LONGEST; ii >= AIR_LORA_MODE_FASTEST; ii--)
    {
        if (modes & (1 << ii))
        {
            *longest = ii;
            longest_ok = true;
            break;
        }
    }
    return fastest_ok && longest_ok;
}

inline unsigned long air_lora_band_frequency(air_lora_band_e band)
{
    switch (band)
    {
#if defined(LORA_BAND_433)
    case AIR_LORA_BAND_433:
        return 433e6;
#endif
#if defined(LORA_BAND_868)
    case AIR_LORA_BAND_868:
        return 868e6;
#endif
#if defined(LORA_BAND_915)
    case AIR_LORA_BAND_915:
        return 915e6;
#endif
    }
    UNREACHABLE();
    return 0;
}

void air_lora_set_parameters(lora_t *lora, air_lora_mode_e mode);
time_micros_t air_lora_full_cycle_time(air_lora_mode_e mode);
time_micros_t air_lora_uplink_cycle_time(air_lora_mode_e mode);
bool air_lora_cycle_is_full(air_lora_mode_e mode, unsigned seq);
time_micros_t air_lora_tx_failsafe_interval(air_lora_mode_e mode);
time_micros_t air_lora_rx_failsafe_interval(air_lora_mode_e mode);

void air_lora_set_parameters_bind(lora_t *lora, air_lora_band_e band);