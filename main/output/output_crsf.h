#pragma once

#include <hal/gpio.h>

#include "msp/msp_telemetry.h"

#include "output/output.h"

#include "protocols/crsf.h"

typedef struct output_crsf_config_s
{
    hal_gpio_t tx;
    hal_gpio_t rx;
    bool inverted;
} output_crsf_config_t;

typedef struct output_crsf_s
{
    output_t output;
    crsf_port_t crsf;
    time_micros_t next_ping;

    msp_telemetry_t msp_telemetry;
} output_crsf_t;

void output_crsf_init(output_crsf_t *output);
