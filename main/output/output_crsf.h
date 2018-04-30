#pragma once

#include <driver/gpio.h>

#include "msp/msp_telemetry.h"

#include "output/output.h"

#include "protocols/crsf.h"

typedef struct output_crsf_config_s
{
    gpio_num_t tx_pin_num;
    gpio_num_t rx_pin_num;
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
