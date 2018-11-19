#include <assert.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>

#include <hal/gpio-private.h>
#include <hal/log.h>
#include <hal/spi.h>

LOG_TAG_DECLARE("SPI");

hal_err_t hal_spi_bus_init(hal_spi_bus_t bus, hal_gpio_t miso, hal_gpio_t mosi, hal_gpio_t sck)
{
    int spi_rcc = 0;
    int gpio_rcc = 0;
    hal_gpio_t gpio_miso = HAL_GPIO_NONE;
    hal_gpio_t gpio_mosi = HAL_GPIO_NONE;
    hal_gpio_t gpio_sck = HAL_GPIO_NONE;
    switch (bus)
    {
    case SPI1:
        // NSS=PA4, SCK=PA5, MISO=PA6 and MOSI=PA7
        gpio_miso = HAL_GPIO_PA(6);
        gpio_mosi = HAL_GPIO_PA(7);
        gpio_sck = HAL_GPIO_PA(5);
        spi_rcc = RCC_SPI1;
        gpio_rcc = RCC_GPIOA;
        break;
    case SPI2:
        // NSS=PB12, SCK=PB13, MISO=PB14, MOSI=PB15
        gpio_miso = HAL_GPIO_PB(14);
        gpio_mosi = HAL_GPIO_PB(15);
        gpio_sck = HAL_GPIO_PB(13);
        spi_rcc = RCC_SPI2;
        gpio_rcc = RCC_GPIOB;
        break;
    case SPI3:
        // NSS=PA15, SCK=PB3, MISO=PB4, MOSI=PB5
        gpio_miso = HAL_GPIO_PB(3);
        gpio_mosi = HAL_GPIO_PB(3);
        gpio_sck = HAL_GPIO_PB(13);
        spi_rcc = RCC_SPI3;
        gpio_rcc = RCC_GPIOB;
        break;
    default:
        LOG_F(TAG, "Unsupported SPI bus %u", (unsigned)bus);
    }
    assert(gpio_miso == miso);
    assert(gpio_mosi == mosi);
    assert(gpio_sck == sck);
    rcc_periph_clock_enable(RCC_AFIO);
    rcc_periph_clock_enable(spi_rcc);
    rcc_periph_clock_enable(gpio_rcc);

    // Configure MISO, MOSI and SCK
    gpio_set_mode(hal_gpio_port(miso), GPIO_MODE_INPUT, GPIO_CNF_INPUT_FLOAT, hal_gpio_bit(miso));

    gpio_set_mode(hal_gpio_port(mosi), GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, hal_gpio_bit(mosi));
    gpio_set_mode(hal_gpio_port(sck), GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, hal_gpio_bit(sck));

    return HAL_ERR_NONE;
}

hal_err_t hal_spi_bus_add_device(hal_spi_bus_t bus, const hal_spi_device_config_t *cfg, hal_spi_device_handle_t *dev)
{
    // Check cmd + addr size
    if (cfg->command_bits + cfg->address_bits != 8)
    {
        LOG_F(TAG, "Unsupported command_bits + address_bits");
    }
#if 0
    // Configure NSS. We allow any NSS pin because we manage it by software.
    uint32_t cs_port = hal_gpio_port(cfg->cs);
    uint32_t cs_bit = hal_gpio_bit(cfg->cs);
    gpio_set_mode(cs_port, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, cs_bit);
    gpio_set(cs_port, cs_bit);
#endif

    spi_reset(bus);

    uint32_t cpol = 0;
    uint32_t cpha = 0;
    switch (cfg->mode)
    {
    case HAL_SPI_MODE_0:
        cpol = SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE;
        cpha = SPI_CR1_CPHA_CLK_TRANSITION_1;
        break;
    case HAL_SPI_MODE_1:
        cpol = SPI_CR1_CPOL_CLK_TO_0_WHEN_IDLE;
        cpha = SPI_CR1_CPHA_CLK_TRANSITION_2;
        break;
    case HAL_SPI_MODE_2:
        cpol = SPI_CR1_CPOL_CLK_TO_1_WHEN_IDLE;
        cpha = SPI_CR1_CPHA_CLK_TRANSITION_1;
        break;
    case HAL_SPI_MODE_3:
        cpol = SPI_CR1_CPOL_CLK_TO_1_WHEN_IDLE;
        cpha = SPI_CR1_CPHA_CLK_TRANSITION_2;
        break;
    }

    // TODO: Clock speed
    spi_init_master(bus, SPI_CR1_BAUDRATE_FPCLK_DIV_16, cpol, cpha, SPI_CR1_DFF_8BIT, SPI_CR1_MSBFIRST);

    spi_disable_software_slave_management(bus);
    spi_enable_ss_output(bus);

    // Manage NSS by software
    //spi_enable_software_slave_management(bus);
    // Enable SPI
    spi_enable(bus);

    dev->bus = bus;
    dev->command_bits = cfg->command_bits;
    dev->address_bits = cfg->address_bits;
    dev->cs = cfg->cs;
    return HAL_ERR_NONE;
}

hal_err_t hal_spi_device_transmit(const hal_spi_device_handle_t *dev, uint16_t cmd, uint32_t addr,
                                  const void *tx, size_t tx_size,
                                  void *rx, size_t rx_size)
{
    if (rx_size == 0)
    {
        rx_size = tx_size;
    }
    uint32_t cs_port = hal_gpio_port(dev->cs);
    uint32_t cs_bit = hal_gpio_bit(dev->cs);
    //gpio_clear(cs_port, cs_bit);

    /* Set NSS management to software.
    *
    * Note:
    * Setting nss high is very important, even if we are controlling the GPIO
    * ourselves this bit needs to be at least set to 1, otherwise the spi
    * peripheral will not send any data out.
    */
    //spi_set_nss_high(dev->bus);

    // Write cmd+addr
    uint8_t cmd_addr = (cmd << dev->address_bits) | addr;
    printf("WRITE CMD %x\n", cmd_addr);
    uint8_t r1 = spi_xfer(dev->bus, cmd_addr);
    printf("R1 %u\n", r1);

    // Now start writing bytes
    const uint8_t *input = tx;
    uint8_t *output = rx;
    if (output)
    {
        for (size_t ii = 0, jj = 0; ii < tx_size; ii++, jj++)
        {
            uint8_t c = *input;
            input++;
            uint8_t r = spi_xfer(dev->bus, c);
            printf("R %u\n", r);
            if (jj < rx_size)
            {
                *output = r;
                output++;
            }
        }
    }
    else
    {
        for (size_t ii = 0; ii < tx_size; ii++)
        {
            uint8_t c = *input;
            input++;
            spi_xfer(dev->bus, c);
        }
    }

    //gpio_set(cs_port, cs_bit);

    //spi_set_nss_low(dev->bus);
    spi_disable(dev->bus);

    return HAL_ERR_NONE;
}

hal_err_t hal_spi_device_transmit_u8(const hal_spi_device_handle_t *dev, uint16_t cmd, uint32_t addr, uint8_t c, uint8_t *out)
{
    return hal_spi_device_transmit(dev, cmd, addr, &c, 1, out, 0);
}