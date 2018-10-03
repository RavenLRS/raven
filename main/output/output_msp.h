#pragma once

#include <hal/gpio.h>

#include "msp/msp.h"
#include "msp/msp_serial.h"
#include "output.h"
#include "time.h"

typedef struct output_msp_config_s
{
    hal_gpio_t tx;
    hal_gpio_t rx;
    msp_serial_baudrate_e baud_rate;
} output_msp_config_t;

#define OUTPUT_MSP_POLL_COUNT 7

typedef struct output_msp_s
{
    output_t output;
    msp_serial_t msp_serial;
    output_msp_poll_t polls[OUTPUT_MSP_POLL_COUNT];
    bool multiwii_current_meter_output;
} output_msp_t;

void output_msp_init(output_msp_t *output);
