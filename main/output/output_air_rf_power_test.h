#pragma once

#include "air/air_io.h"
#include "air/air_lora.h"

#include "util/time.h"

#include "output/output.h"

typedef struct lora_s lora_t;

typedef struct output_air_rf_power_test_s
{
    output_t output;
    lora_t *lora;
    air_lora_band_e band;
    time_micros_t next_switch;
    int state;
    int power;
} output_air_rf_power_test_t;

void output_air_rf_power_test_init(output_air_rf_power_test_t *output, lora_t *lora, air_lora_band_e band);