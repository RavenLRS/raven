#pragma once

#include <driver/i2c.h>

typedef i2c_port_t hal_i2c_bus_t;

typedef struct hal_i2c_cmd_s
{
    i2c_cmd_handle_t handle;
} hal_i2c_cmd_t;

#include <hal/i2c_base.h>