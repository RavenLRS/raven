#pragma once

#include <stdint.h>

#include <hal/gpio.h>

typedef uint32_t hal_spi_bus_t;

typedef struct hal_spi_device_handle_s
{
    hal_spi_bus_t bus;
    uint8_t command_bits;
    uint8_t address_bits;
    hal_gpio_t cs;
} hal_spi_device_handle_t;

#include <hal/spi_base.h>