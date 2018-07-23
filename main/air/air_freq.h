#pragma once

#include <stdint.h>

#include "air/air.h"

typedef struct air_freq_table_s
{
    unsigned long freqs[AIR_NUM_HOPPING_FREQS];
    int abs_errors[AIR_NUM_HOPPING_FREQS];
    int last_errors[AIR_NUM_HOPPING_FREQS];
} air_freq_table_t;

void air_freq_table_init(air_freq_table_t *tbl, air_key_t key, unsigned long base_freq);
