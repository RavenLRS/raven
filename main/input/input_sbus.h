#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "input/input.h"

#include "io/gpio.h"
#include "io/serial.h"

#include "protocols/sbus.h"

#include "util/time.h"

typedef struct input_sbus_config_s
{
    hal_gpio_t rx;
} input_sbus_config_t;

typedef struct input_sbus_s
{
    input_t input;
    serial_port_t *serial_port;
    hal_gpio_t rx;
    sbus_frame_t frame;
    uint8_t frame_pos;
    time_micros_t frame_end;
    bool inverted;
    time_micros_t next_inversion_switch;
} input_sbus_t;

void input_sbus_init(input_sbus_t *input);