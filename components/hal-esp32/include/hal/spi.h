#pragma once

#include <driver/spi_master.h>

typedef spi_host_device_t hal_spi_bus_t;

typedef struct hal_spi_device_handle_s
{
    spi_device_handle_t dev;
} hal_spi_device_handle_t;

#include <hal/spi_base.h>