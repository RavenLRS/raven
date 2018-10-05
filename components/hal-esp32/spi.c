#include <string.h>

#include <hal/spi.h>

hal_err_t hal_spi_bus_init(hal_spi_bus_t bus, hal_gpio_t miso, hal_gpio_t mosi, hal_gpio_t sck)
{
    spi_bus_config_t cfg = {
        .miso_io_num = miso,
        .mosi_io_num = mosi,
        .sclk_io_num = sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
    };
    return spi_bus_initialize(bus, &cfg, 1);
}

hal_err_t hal_spi_bus_add_device(hal_spi_bus_t bus, const hal_spi_device_config_t *cfg, hal_spi_device_handle_t *dev)
{
    spi_device_interface_config_t devcfg;
    memset(&devcfg, 0, sizeof(devcfg));
    devcfg.command_bits = cfg->command_bits;
    devcfg.address_bits = cfg->address_bits;
    devcfg.clock_speed_hz = cfg->clock_speed_hz;
    devcfg.mode = 0; // SPI mode 0
    devcfg.spics_io_num = cfg->cs;
    devcfg.queue_size = 4;
    // Attach the device
    return spi_bus_add_device(bus, &devcfg, &dev->dev);
}

hal_err_t hal_spi_device_transmit(const hal_spi_device_handle_t *dev, uint16_t cmd, uint32_t addr,
                                  const void *tx, size_t tx_size,
                                  void *rx, size_t rx_size)
{
    // We don't use DMA enabled memory here because allocating DMA enabled
    // memory actually makes things a bit slower for the smallish payloads
    // we send (16 bytes at most right now).
    spi_transaction_t t = {
        .cmd = cmd,
        .addr = addr,
        .length = tx_size * 8,
        .rxlength = rx_size * 8,
        .tx_buffer = tx,
        .rx_buffer = rx,
        .flags = 0,
    };
    return spi_device_transmit(dev->dev, &t);
}

hal_err_t hal_spi_device_transmit_u8(const hal_spi_device_handle_t *dev, uint16_t cmd, uint32_t addr, uint8_t c, uint8_t *out)
{
    spi_transaction_t t = {
        .cmd = cmd,
        .addr = addr,
        .length = 8,
        .rxlength = 8,
        .flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA,
    };
    t.tx_data[0] = c;
    esp_err_t err = spi_device_transmit(dev->dev, &t);
    if (out)
    {
        *out = t.rx_data[0];
    }
    return err;
}