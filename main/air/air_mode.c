#include "air/air_mode.h"

bool air_mode_is_valid(air_mode_e mode)
{
    return mode >= AIR_MODE_FASTEST && mode <= AIR_MODE_LONGEST;
}

static air_mode_e _air_mode_faster(air_mode_e mode)
{
    if (mode > AIR_MODE_FASTEST)
    {
        return mode - 1;
    }
    return AIR_MODE_INVALID;
}

static air_mode_e _air_mode_longer(air_mode_e mode)
{
    if (mode < AIR_MODE_LONGEST)
    {
        return mode + 1;
    }
    return AIR_MODE_INVALID;
}

air_mode_e air_mode_faster(air_mode_e mode, air_mode_mask_t supported)
{
    while (1)
    {
        air_mode_e faster = _air_mode_faster(mode);
        if (!air_mode_is_valid(faster))
        {
            // No faster modes left
            break;
        }
        if (air_mode_mask_contains(supported, faster))
        {
            return faster;
        }
        mode = faster;
    }
    return AIR_MODE_INVALID;
}

air_mode_e air_mode_longer(air_mode_e mode, air_mode_mask_t supported)
{
    while (1)
    {
        air_mode_e longer = _air_mode_longer(mode);
        if (!air_mode_is_valid(longer))
        {
            // No faster modes left
            break;
        }
        if (air_mode_mask_contains(supported, longer))
        {
            return longer;
        }
        mode = longer;
    }
    return AIR_MODE_INVALID;
}

air_mode_e air_mode_fastest(air_mode_mask_t supported)
{
    for (int ii = AIR_MODE_FASTEST; ii <= AIR_MODE_LONGEST; ii++)
    {
        if (air_mode_mask_contains(supported, ii))
        {
            return ii;
        }
    }
    return AIR_MODE_INVALID;
}

air_mode_e air_mode_longest(air_mode_mask_t supported)
{
    for (int ii = AIR_MODE_LONGEST; ii >= AIR_MODE_FASTEST; ii--)
    {
        if (air_mode_mask_contains(supported, ii))
        {
            return ii;
        }
    }
    return AIR_MODE_INVALID;
}

bool air_mode_mask_contains(air_mode_mask_t mask, air_mode_e mode)
{
    return mask & AIR_MODE_BIT(mode);
}

air_mode_mask_t air_mode_mask_remove(air_mode_mask_t mask, air_mode_e mode)
{
    return mask & ~AIR_MODE_BIT(mode);
}

air_mode_mask_t air_modes_pack(air_supported_modes_e supported)
{
    air_mode_mask_t modes = 0;
    switch (supported)
    {
    case AIR_SUPPORTED_MODES_FIXED_1:
        modes |= AIR_MODE_BIT(AIR_MODE_1);
        break;
    case AIR_SUPPORTED_MODES_FIXED_2:
        modes |= AIR_MODE_BIT(AIR_MODE_2);
        break;
    case AIR_SUPPORTED_MODES_FIXED_3:
        modes |= AIR_MODE_BIT(AIR_MODE_3);
        break;
    case AIR_SUPPORTED_MODES_FIXED_4:
        modes |= AIR_MODE_BIT(AIR_MODE_4);
        break;
    case AIR_SUPPORTED_MODES_FIXED_5:
        modes |= AIR_MODE_BIT(AIR_MODE_5);
        break;
    case AIR_SUPPORTED_MODES_1_TO_5:
        for (int ii = AIR_MODE_1; ii <= AIR_MODE_5; ii++)
        {
            modes |= AIR_MODE_BIT(ii);
        }
        break;
    case AIR_SUPPORTED_MODES_2_TO_5:
        for (int ii = AIR_MODE_2; ii <= AIR_MODE_5; ii++)
        {
            modes |= AIR_MODE_BIT(ii);
        }
        break;
    }
    return modes;
}

bool air_modes_intersect(air_mode_mask_t *intersection, air_supported_modes_e s1, air_supported_modes_e s2)
{
    air_mode_mask_t i = air_modes_pack(s1) & air_modes_pack(s2);
    if (intersection)
    {
        *intersection = i;
    }
    return i != 0;
}