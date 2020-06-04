#pragma once

#include <stdbool.h>
#include <stdint.h>

#define AIR_MODE_INVALID 0
#define AIR_MODE_BIT(mode) (1 << mode)
#define AIR_MODE_COUNT (AIR_MODE_LONGEST - AIR_MODE_FASTEST + 1)

typedef enum
{
    AIR_MODE_1 = 1, // SF6, BW500 - 10ms cycle
    AIR_MODE_2,     // SF7, BW500 - 17ms cycle
    AIR_MODE_3,     // SF8, BW500 - 31ms cycle
    AIR_MODE_4,     // SF9, BW500 - 55ms cycle
    AIR_MODE_5,     // SF10, BW500 - 110ms cycle
    AIR_MODE_FASTEST = AIR_MODE_1,
    AIR_MODE_LONGEST = AIR_MODE_5,
} air_mode_e;

typedef uint16_t air_mode_mask_t;

typedef enum
{
    AIR_SUPPORTED_MODES_FIXED_1 = 1,
    AIR_SUPPORTED_MODES_FIXED_2 = 2,
    AIR_SUPPORTED_MODES_FIXED_3 = 3,
    AIR_SUPPORTED_MODES_FIXED_4 = 4,
    AIR_SUPPORTED_MODES_FIXED_5 = 5,
    AIR_SUPPORTED_MODES_1_TO_5 = 32,
    AIR_SUPPORTED_MODES_2_TO_5 = 33,
} air_supported_modes_e;

bool air_mode_is_valid(air_mode_e mode);
air_mode_e air_mode_faster(air_mode_e mode, air_mode_mask_t supported);
air_mode_e air_mode_longer(air_mode_e mode, air_mode_mask_t supported);
air_mode_e air_mode_fastest(air_mode_mask_t supported);
air_mode_e air_mode_longest(air_mode_mask_t supported);

bool air_mode_mask_contains(air_mode_mask_t mask, air_mode_e mode);
air_mode_mask_t air_mode_mask_remove(air_mode_mask_t mask, air_mode_e mode);
air_mode_mask_t air_modes_pack(air_supported_modes_e supported);
bool air_modes_intersect(air_mode_mask_t *intersection, air_supported_modes_e s1, air_supported_modes_e s2);
