#pragma once

#include <hal/gpio.h>

#include "output/output.h"

#include "protocols/smartport.h"

typedef struct output_sbus_config_s
{
    hal_gpio_t sbus;
    bool sbus_inverted;
    hal_gpio_t sport;
    bool sport_inverted;
} output_sbus_config_t;

typedef struct output_sbus_s
{
    output_t output;
    serial_port_t *sport_serial_port;
    smartport_master_t sport_master;
} output_sbus_t;

void output_sbus_init(output_sbus_t *output);
