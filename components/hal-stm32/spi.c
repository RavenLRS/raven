#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/spi.h>

#include <hal/gpio-private.h>
#include <hal/log.h>
#include <hal/spi.h>

LOG_TAG_DECLARE("SPI");

#define HAL_SPI_PRESCALER_MIN 0 // /2 prescaling
#define HAL_SPI_PRESCALER_MAX 7 // /256 prescaling

static uint32_t hal_spi_base_frequency(hal_spi_bus_t bus)
{
    switch (bus)
    {
    case SPI1:
        return rcc_apb2_frequency;
    case SPI2:
        return rcc_apb1_frequency;
    }
    LOG_F(TAG, "Invalid SPI BUS");
    return 0;
}

static unsigned hal_spi_select_prescaler(hal_spi_bus_t bus, uint32_t clock_speed_hz)
{
    // Calculate clock rate
    uint32_t base_freq = hal_spi_base_frequency(bus);
    for (int ii = HAL_SPI_PRESCALER_MIN; ii <= HAL_SPI_PRESCALER_MAX; ii++)
    {
        // Return the first prescaler that results on clock speed <= clock_speed_hz
        uint32_t prescaled_freq = base_freq / (1 << (ii + 1));
        if (prescaled_freq <= clock_speed_hz)
        {
            return ii;
        }
    }
    return HAL_SPI_PRESCALER_MAX;
}

hal_err_t hal_spi_bus_init(hal_spi_bus_t bus, hal_gpio_t miso, hal_gpio_t mosi, hal_gpio_t sck)
{
    int spi_rcc = 0;
    int gpio_rcc = 0;
    hal_gpio_t gpio_miso;
    hal_gpio_t gpio_mosi;
    hal_gpio_t gpio_sck;
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
    default:
        LOG_F(TAG, "Unsupported SPI bus %u", (unsigned)bus);
    }

    if (gpio_miso != miso || gpio_mosi != mosi || gpio_sck != sck)
    {
        return HAL_ERR_INVALID_ARG;
    }

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

    // Configure NSS. We allow any NSS pin because we manage it by software.
    uint32_t cs_port = hal_gpio_port(cfg->cs);
    uint32_t cs_bit = hal_gpio_bit(cfg->cs);
    gpio_set_mode(cs_port, GPIO_MODE_OUTPUT_50_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, cs_bit);
    gpio_set(cs_port, cs_bit);

    spi_reset(bus);

    uint32_t cr1 = SPI_CR1(bus);
    /* Reset all bits omitting SPE, CRCEN and CRCNEXT bits (from spi_init_master()). */
    cr1 &= SPI_CR1_SPE | SPI_CR1_CRCEN | SPI_CR1_CRCNEXT;
    // Enable master mode
    cr1 |= SPI_CR1_MSTR;

    // 8 bit data frame format and MSB first are the defaults.
    // We handle clock divisor, cpol and cpha in hal_spi_device_transmit()
    SPI_CR1(bus) = cr1;

    // Manage NSS by software
    spi_enable_software_slave_management(bus);
    spi_set_nss_high(bus);

    dev->bus = bus;
    // We can assing directly since hal_spi_mode_t matches
    // the format of the argument for spi_set_standard_mode()
    dev->mode = cfg->mode;
    dev->prescaler = hal_spi_select_prescaler(bus, cfg->clock_speed_hz);
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
    gpio_clear(cs_port, cs_bit);

    spi_set_baudrate_prescaler(dev->bus, dev->prescaler);
    spi_set_standard_mode(dev->bus, dev->mode);

    spi_enable(dev->bus);

    // Write cmd+addr
    uint8_t cmd_addr = (cmd << dev->address_bits) | addr;
    spi_xfer(dev->bus, cmd_addr);

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

    gpio_set(cs_port, cs_bit);

    spi_disable(dev->bus);

    return HAL_ERR_NONE;
}

hal_err_t hal_spi_device_transmit_u8(const hal_spi_device_handle_t *dev, uint16_t cmd, uint32_t addr, uint8_t c, uint8_t *out)
{
    return hal_spi_device_transmit(dev, cmd, addr, &c, 1, out, 0);
}