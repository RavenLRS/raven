#pragma once

#include "air/air_band.h"
#include "air/air_mode.h"

typedef struct air_radio_s air_radio_t;

typedef struct air_config_s
{
    air_radio_t *radio;
    air_supported_modes_e modes;
    air_band_e band;
    air_band_mask_t bands;
} air_config_t;
