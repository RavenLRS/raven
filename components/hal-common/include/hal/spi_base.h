#pragma once

#include <stdint.h>

#include <hal/err.h>
#include <hal/gpio.h>

typedef struct hal_spi_device_config_s
{
    uint8_t command_bits;
    uint8_t address_bits;
    uint32_t clock_speed_hz;
    hal_gpio_t cs;
} hal_spi_device_config_t;

hal_err_t hal_spi_bus_init(hal_spi_bus_t bus, hal_gpio_t miso, hal_gpio_t mosi, hal_gpio_t sck);
hal_err_t hal_spi_bus_add_device(hal_spi_bus_t bus, const hal_spi_device_config_t *cfg, hal_spi_device_handle_t *dev);
// tx_size and rx_size are expressed in bytes. rx_size must be <= tx_size.
// Passing rx_size == 0 means rx_size is the same as tx_size.
hal_err_t hal_spi_device_transmit(const hal_spi_device_handle_t *dev, uint16_t cmd, uint32_t addr,
                                  const void *tx, size_t tx_size,
                                  void *rx, size_t rx_size);
hal_err_t hal_spi_device_transmit_u8(const hal_spi_device_handle_t *dev, uint16_t cmd, uint32_t addr, uint8_t c, uint8_t *out);