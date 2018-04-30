#pragma once

#include <driver/gpio.h>

#include "output/output.h"

#include "protocols/smartport.h"

typedef struct output_sbus_config_s
{
    gpio_num_t sbus_pin_num;
    bool sbus_inverted;
    gpio_num_t sport_pin_num;
    bool sport_inverted;
} output_sbus_config_t;

typedef struct output_sbus_s
{
    output_t output;
    serial_port_t *sport_serial_port;
    smartport_master_t sport_master;
} output_sbus_t;

void output_sbus_init(output_sbus_t *output);
