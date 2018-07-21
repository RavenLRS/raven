#pragma once

#include "io/sx127x.h"

typedef struct air_radio_s
{
    sx127x_t sx127x;
} air_radio_t;