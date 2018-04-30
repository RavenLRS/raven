#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <driver/gpio.h>

#include "output/output.h"

#include "msp/msp_telemetry.h"

#include "protocols/smartport.h"

#define OUTPUT_FPORT_BUFSIZE 64

typedef struct output_fport_config_s
{
    gpio_num_t tx_pin_num;
    gpio_num_t rx_pin_num;
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
