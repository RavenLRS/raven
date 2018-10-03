#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <hal/gpio.h>

#include "output/output.h"

#include "msp/msp_telemetry.h"

#include "protocols/smartport.h"

#define OUTPUT_FPORT_BUFSIZE 64

typedef struct output_fport_config_s
{
    hal_gpio_t tx;
    hal_gpio_t rx;
    bool inverted;
} output_fport_config_t;

typedef struct output_fport_s
{
    output_t output;

    smartport_master_t sport_master;
    msp_telemetry_t msp_telemetry;
    uint8_t buf[OUTPUT_FPORT_BUFSIZE];
    unsigned buf_pos;
} output_fport_t;

void output_fport_init(output_fport_t *output);
