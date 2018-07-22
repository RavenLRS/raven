#include <math.h>
#include <stdint.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_heap_caps.h>
#include <esp_system.h>

#include <driver/gpio.h>
#include <driver/spi_master.h>

#include <hal/log.h>

#include "util/macros.h"
#include "util/time.h"

#include "sx127x.h"

// registers
#define REG_FIFO 0x00
#define REG_OP_MODE 0x01
#define REG_FRF_MSB 0x06
#define REG_FRF_MID 0x07
#define REG_FRF_LSB 0x08
#define REG_PA_CONFIG 0x09
#define REG_LNA 0x0c
#define REG_FIFO_ADDR_PTR 0x0d
#define REG_FIFO_TX_BASE_ADDR 0x0e
#define REG_FIFO_RX_BASE_ADDR 0x0f
#define REG_FIFO_RX_CURRENT_ADDR 0x10
#define REG_IRQ_FLAGS 0x12
#define REG_RX_NB_BYTES 0x13
#define REG_PKT_SNR_VALUE 0x19
#define REG_PKT_RSSI_VALUE 0x1a
#define REG_MODEM_CONFIG_1 0x1d
#define REG_MODEM_CONFIG_2 0x1e
#define REG_PREAMBLE_MSB 0x20
#define REG_PREAMBLE_LSB 0x21
#define REG_PAYLOAD_LENGTH 0x22
#define REG_MODEM_CONFIG_3 0x26
#define REG_RSSI_WIDEBAND 0x2c
#define REG_DETECTION_OPTIMIZE 0x31
#define REG_DETECTION_THRESHOLD 0x37
#define REG_SYNC_WORD 0x39
#define REG_DIO_MAPPING_1 0x40
#define REG_DIO_MAPPING_2 0x41
#define REG_VERSION 0x42
#define REG_PA_DAC 0x4d

// modes
#define MODE_LONG_RANGE_MODE 0x80
#define MODE_SLEEP 0x00
#define MODE_STDBY 0x01
#define MODE_TX 0x03
#define MODE_RX_CONTINUOUS 0x05
#define MODE_RX_SINGLE 0x06

// PA config
#define PA_BOOST 0x80

// IRQ masks
#define IRQ_TX_DONE_MASK 0x08
#define IRQ_PAYLOAD_CRC_ERROR_MASK 0x20
#define IRQ_RX_DONE_MASK 0x40

// Page 46, table 18 indicates the DIO0 values,
// page 92 indicates that DIO0 is in the most
// significant 2 bits
#define DIO0_RX_DONE (0 << 6)
#define DIO0_TX_DONE (1 << 6)
#define DIO0_NONE (3 << 6)

#define TX_FIFO_ADDR 0x80
#define RX_FIFO_ADDR 0

#define SX127X_EXPECTED_VERSION 18

enum
{
    DIO0_TRIGGER_RX_DONE = 1,
    DIO0_TRIGGER_TX_DONE,
};

static const char *TAG = "SX127X";

static esp_err_t spi_device_transmit_sync(spi_device_handle_t handle, spi_transaction_t *trans_desc)
{
    // Just a wrapper for now
    return spi_device_transmit(handle, trans_desc);
}

static uint8_t sx127x_read_reg(sx127x_t *sx127x, uint8_t addr)
{
    spi_transaction_t t;
    t.cmd = 0;
    t.addr = addr;
    t.length = 8; // Send 8 arbitrary bits to get one byte back in full duplex
    t.rxlength = 0;
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    ESP_ERROR_CHECK(spi_device_transmit_sync(sx127x->state.spi, &t));
    return t.rx_data[0];
}

static inline void sx127x_write_prepare_spi_transaction(spi_transaction_t *t, uint8_t addr, uint8_t value)
{
    t->cmd = 1;
    t->addr = addr;
    t->length = 8; // Value byte is 8 bits
    t->rxlength = 0;
    t->flags = SPI_TRANS_USE_TXDATA;
    t->tx_data[0] = value;
    t->rx_buffer = NULL;
}

static void sx127x_write_reg(sx127x_t *sx127x, uint8_t addr, uint8_t value)
{
    spi_transaction_t t;
    sx127x_write_prepare_spi_transaction(&t, addr, value);
    ESP_ERROR_CHECK(spi_device_transmit_sync(sx127x->state.spi, &t));
}

static void sx127x_set_mode(sx127x_t *sx127x, uint8_t mode)
{
    if (sx127x->state.mode != mode)
    {
        sx127x_write_reg(sx127x, REG_OP_MODE, mode);
        sx127x->state.mode = mode;
    }
}

static void sx127x_prepare_write(sx127x_t *sx127x)
{
    // Registers can only be written to when the device is
    // in sleep or idle mode. If it's not, we set it to idle
    uint8_t mode = sx127x->state.mode & ~MODE_LONG_RANGE_MODE;
    if (mode != MODE_SLEEP && mode != MODE_STDBY)
    {
        sx127x_idle(sx127x);
    }
}

static int link_quality(int min_dbm, int max_dbm, int dbm)
{
    return (100 * (max_dbm - min_dbm) * (max_dbm - min_dbm) - (max_dbm - dbm) * (25 * (max_dbm - min_dbm) + 75 * (max_dbm - dbm))) / ((max_dbm - min_dbm) * (max_dbm - min_dbm));
}

static TaskHandle_t callback_task_handle = NULL;

static void sx127x_callback_task(void *arg)
{
    sx127x_t *sx127x = arg;
    air_radio_callback_t callback;
    for (;;)
    {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        switch (sx127x->state.dio0_trigger)
        {
        case DIO0_TRIGGER_RX_DONE:
            sx127x->state.rx_done = true;
            if (sx127x->state.callback)
            {
                callback = sx127x->state.callback;
                callback((air_radio_t *)sx127x, AIR_RADIO_CALLBACK_REASON_RX_DONE, sx127x->state.callback_data);
            }
            break;
        case DIO0_TRIGGER_TX_DONE:
            sx127x->state.tx_done = true;
            if (sx127x->state.callback)
            {
                callback = sx127x->state.callback;
                callback((air_radio_t *)sx127x, AIR_RADIO_CALLBACK_REASON_TX_DONE, sx127x->state.callback_data);
            }
            break;
        }
    }
}

static void IRAM_ATTR lora_handle_isr(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(callback_task_handle, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

void sx127x_init(sx127x_t *sx127x)
{
    gpio_set_direction(sx127x->rst, GPIO_MODE_OUTPUT);
    gpio_set_level(sx127x->rst, 0);
    vTaskDelay(20 / portTICK_PERIOD_MS);
    gpio_set_level(sx127x->rst, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);

    gpio_set_direction(sx127x->cs, GPIO_MODE_OUTPUT);

    spi_bus_config_t buscfg = {
        .miso_io_num = sx127x->miso,
        .mosi_io_num = sx127x->mosi,
        .sclk_io_num = sx127x->sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0,
    };
    spi_device_interface_config_t devcfg;
    memset(&devcfg, 0, sizeof(devcfg));
    devcfg.command_bits = 1;                 // 1 command bit, 1 => write, 0 => read
    devcfg.address_bits = 7;                 // 7 addr bits
    devcfg.clock_speed_hz = 9 * 1000 * 1000; // Clock out at 9 MHz: XXX => 10Mhz will cause incorrect reads from REG_MODEM_CONFIG_1
    devcfg.mode = 0;                         // SPI mode 0
    devcfg.spics_io_num = sx127x->cs;        // CS pin
    devcfg.queue_size = 4;
    //Initialize the SPI bus
    ESP_ERROR_CHECK(spi_bus_initialize(VSPI_HOST, &buscfg, 1));
    // Attach the device
    ESP_ERROR_CHECK(spi_bus_add_device(VSPI_HOST, &devcfg, &sx127x->state.spi));

    sx127x->state.tx_done = false;
    sx127x->state.rx_done = false;
    sx127x->state.freq = 0;

    xTaskCreatePinnedToCore(sx127x_callback_task, "SX127X-CALLBACK", 4096, sx127x, 1000, &callback_task_handle, 1);

    uint8_t version = sx127x_read_reg(sx127x, REG_VERSION);
    if (version == SX127X_EXPECTED_VERSION)
    {
        LOG_I(TAG, "Got SX127X chip version %u", version);
    }
    else
    {
        LOG_E(TAG, "Unexpected SX127X chip version %u, expecting %d", version, SX127X_EXPECTED_VERSION);
        UNREACHABLE();
    }

    sx127x->state.mode = sx127x_read_reg(sx127x, REG_OP_MODE);
    sx127x->state.payload_size = sx127x_read_reg(sx127x, REG_PAYLOAD_LENGTH);
    sx127x->state.callback = NULL;

    // Put it in sleep mode to change some registers
    sx127x_sleep(sx127x);

    sx127x_write_reg(sx127x, REG_FIFO_TX_BASE_ADDR, TX_FIFO_ADDR);
    sx127x_write_reg(sx127x, REG_FIFO_RX_BASE_ADDR, RX_FIFO_ADDR);

    // LNA boost
    sx127x_write_reg(sx127x, REG_LNA, sx127x_read_reg(sx127x, REG_LNA) | 0x03);

    // set auto AGC
    sx127x_write_reg(sx127x, REG_MODEM_CONFIG_3, 0x04);

#if defined(CONFIG_RAVEN_DIO5_CLK_OUTPUT)
    // Enable DIO5 as ClkOut
    uint8_t dio_mapping_2 = sx127x_read_reg(sx127x, REG_DIO_MAPPING_2);
    sx127x_write_reg(sx127x, REG_DIO_MAPPING_2, dio_mapping_2 | (1 << 5));
#endif

    // set output power to 17 dBm
    sx127x_set_tx_power(sx127x, 17);

    sx127x_set_lora_coding_rate(sx127x, SX127X_LORA_CODING_RATE_4_5);
    sx127x_set_lora_spreading_factor(sx127x, 8);
    sx127x_set_lora_signal_bw(sx127x, SX127X_LORA_SIGNAL_BW_500);

    // put in standby mode
    sx127x_idle(sx127x);

    // configure pin for ISR
    ESP_ERROR_CHECK(gpio_set_direction(sx127x->dio0, GPIO_MODE_INPUT));
    ESP_ERROR_CHECK(gpio_set_pull_mode(sx127x->dio0, GPIO_FLOATING));
    ESP_ERROR_CHECK(gpio_set_intr_type(sx127x->dio0, GPIO_INTR_POSEDGE));
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_IRAM));

    ESP_ERROR_CHECK(gpio_isr_handler_add(sx127x->dio0, lora_handle_isr, sx127x));
    sx127x->state.dio0_trigger = 0;

    sx127x_write_reg(sx127x, REG_DIO_MAPPING_1, DIO0_NONE);
}

// freq is in Hz
void sx127x_set_frequency(sx127x_t *sx127x, unsigned long freq)
{
    if (freq != sx127x->state.freq)
    {
        sx127x_prepare_write(sx127x);

        uint64_t frf = ((uint64_t)freq << 19) / 32000000;

        sx127x_write_reg(sx127x, REG_FRF_MSB, (uint8_t)(frf >> 16));
        sx127x_write_reg(sx127x, REG_FRF_MID, (uint8_t)(frf >> 8));
        sx127x_write_reg(sx127x, REG_FRF_LSB, (uint8_t)(frf >> 0));
        sx127x->state.freq = freq;
        // Wait up to 50us for PLL lock (page 15, table 7)
        time_micros_t now = time_micros_now();
        do
        {
        } while (time_micros_now() < now + 50);
    }
}

void sx127x_set_payload_size(sx127x_t *sx127x, uint8_t size)
{
    if (sx127x->state.payload_size != size)
    {
        sx127x_prepare_write(sx127x);
        sx127x_write_reg(sx127x, REG_PAYLOAD_LENGTH, size);
        sx127x->state.payload_size = size;
    }
}

void sx127x_sleep(sx127x_t *sx127x)
{
    sx127x_set_mode(sx127x, MODE_LONG_RANGE_MODE | MODE_SLEEP);
}

void sx127x_idle(sx127x_t *sx127x)
{
    sx127x_set_mode(sx127x, MODE_LONG_RANGE_MODE | MODE_STDBY);
}

void sx127x_set_tx_power(sx127x_t *sx127x, int level)
{
    sx127x_prepare_write(sx127x);

    uint8_t pa_config = 0;
    uint8_t pa_dac = 0x84; // default for +17dbm
    switch (sx127x->output_type)
    {
    case SX127X_OUTPUT_RFO:
        if (level < 0)
        {
            level = 0;
        }
        else if (level > 14)
        {
            level = 14;
        }
        pa_config = 0x70 | level;
        break;
    case SX127X_OUTPUT_PA_BOOST:
        if (level < 2)
        {
            level = 2;
        }
        else if (level > 17)
        {
            level = 17;
            pa_dac = 0x87; // Enable +20dbm as Pmax with PA_BOOST
        }
        pa_config = PA_BOOST | (level - 2);
        break;
    default:
        UNREACHABLE();
    }
    sx127x_write_reg(sx127x, REG_PA_CONFIG, pa_config);
    sx127x_write_reg(sx127x, REG_PA_DAC, pa_dac);
}

void sx127x_send(sx127x_t *sx127x, const void *buf, size_t size)
{
    // We need to be in IDLE rather, SLEEP won't work because
    // the FIFO is not available in SLEEP mode.
    sx127x_idle(sx127x);
    // reset FIFO address and payload length
    sx127x_write_reg(sx127x, REG_FIFO_ADDR_PTR, TX_FIFO_ADDR);
    //sx127x_write_reg(sx127x, REG_PAYLOAD_LENGTH, 0);

    // Write payload
    spi_transaction_t t;
    t.cmd = 1;
    t.addr = REG_FIFO;
    t.length = size * 8;
    t.rxlength = 0;
    t.rx_buffer = NULL;
    t.tx_buffer = buf;
    t.flags = 0;
    ESP_ERROR_CHECK(spi_device_transmit_sync(sx127x->state.spi, &t));

    // Update length
    sx127x_set_payload_size(sx127x, size);

    sx127x->state.tx_done = false;
    sx127x_write_reg(sx127x, REG_IRQ_FLAGS, IRQ_TX_DONE_MASK);
    sx127x_write_reg(sx127x, REG_DIO_MAPPING_1, DIO0_TX_DONE);
    sx127x->state.dio0_trigger = DIO0_TRIGGER_TX_DONE;
    // put in TX mode
    sx127x_set_mode(sx127x, MODE_LONG_RANGE_MODE | MODE_TX);
}

size_t sx127x_read(sx127x_t *sx127x, void *buf, size_t size)
{
    sx127x_prepare_write(sx127x);
    sx127x_write_reg(sx127x, REG_FIFO_ADDR_PTR, RX_FIFO_ADDR);
    // For these small transfers, allocating DMA enabled
    // memory actually makes things a bit slower.
    spi_transaction_t t;
    t.flags = 0;
    t.cmd = 0;
    t.addr = REG_FIFO;
    t.length = size * 8;
    t.rxlength = 0;
    t.tx_buffer = NULL;
    t.rx_buffer = buf;
    ESP_ERROR_CHECK(spi_device_transmit_sync(sx127x->state.spi, &t));
    sx127x->state.rx_done = false;
    sx127x_write_reg(sx127x, REG_IRQ_FLAGS, IRQ_RX_DONE_MASK);
    return size;
}

void sx127x_enable_continous_rx(sx127x_t *sx127x)
{
    sx127x_prepare_write(sx127x);
    sx127x->state.rx_done = false;
    sx127x_write_reg(sx127x, REG_DIO_MAPPING_1, DIO0_RX_DONE);
    sx127x->state.dio0_trigger = DIO0_TRIGGER_RX_DONE;

    // Enter continous rx mode
    sx127x_set_mode(sx127x, MODE_LONG_RANGE_MODE | MODE_RX_CONTINUOUS);
}

bool sx127x_is_tx_done(sx127x_t *sx127x)
{
    return sx127x->state.tx_done;
}

bool sx127x_is_rx_done(sx127x_t *sx127x)
{
    return sx127x->state.rx_done;
}

void sx127x_set_callback(sx127x_t *sx127x, air_radio_callback_t callback, void *callback_data)
{
    sx127x->state.callback = callback;
    sx127x->state.callback_data = callback_data;
}

int sx127x_rssi(sx127x_t *sx127x, int *snr, int *lq)
{
    // REG_PKT_SNR_VALUE and REG_PKT_RSSI_VALUE are after each other, so
    // we can read both with a single spi transaction
    _Static_assert(REG_PKT_SNR_VALUE + 1 == REG_PKT_RSSI_VALUE, "REG_PKT_SNR_VALUE and REG_PKT_RSSI_VALUE not contiguous");

    spi_transaction_t t;
    t.cmd = 0;
    t.addr = REG_PKT_SNR_VALUE;
    t.length = 16;
    t.rxlength = 0;
    t.tx_buffer = NULL;
    t.flags = SPI_TRANS_USE_RXDATA;
    ESP_ERROR_CHECK(spi_device_transmit_sync(sx127x->state.spi, &t));
    int8_t raw_snr = (int8_t)t.rx_data[0];
    uint8_t raw_rssi = t.rx_data[1];
    int rssi;
    int min_rssi = sx127x_lora_min_rssi(sx127x);
    if (raw_snr >= 0)
    {
        // Page 87: "- When SNR>=0, the standard formula can be adjusted to
        // correct the slope: RSSI = -157+16/15 * PacketRssi
        // (or RSSI = -164+16/15 * PacketRssi)"
        rssi = min_rssi + (16 / 15.0) * raw_rssi;
    }
    else if (raw_snr < 0)
    {
        // "Packet Strength (dBm) = -157 + PacketRssi + PacketSnr * 0.25 (when using the HF port and SNR < 0)"
        // Same for LF port
        rssi = min_rssi + raw_rssi + raw_snr * 0.25f;
    }
    else
    {
        // Just add them
        rssi = min_rssi + raw_rssi;
    }

    // SNR is multipled by 4 in the register
    if (snr)
    {
        *snr = raw_snr;
    }
    if (lq)
    {
        // Assume max rssi is 1db. According to Pawel's tests
        // on 868MHz link stops working at around ~40 in the
        // register and reports up to ~165 (that'd be ~1db in
        // the HF port). However, testing on 433Mhz reveals up
        // to 9db when antennas are parallel and pretty close
        // at 100mw output.
        // With that said, we're more interested in granularity
        // in the lower RSSI levels, since that's what we're
        // using to switch modes to extend range as needed, so
        // we're using 1 as the max and constraining the result
        // to [0, 100].
        int s = sx127x_lora_rx_sensitivity(sx127x);
        *lq = MAX(0, MIN(link_quality(s, 1, rssi), 100));
    }
    return rssi;
}

void sx127x_shutdown(sx127x_t *sx127x)
{
    sx127x_idle(sx127x);
    gpio_set_level(sx127x->rst, 0);
}

// #pragma region LoRa specific functions

void sx127x_set_lora_spreading_factor(sx127x_t *sx127x, int sf)
{
    sx127x_prepare_write(sx127x);

    if (sf < 6)
    {
        sf = 6;
    }
    else if (sf > 12)
    {
        sf = 12;
    }

    if (sf == 6)
    {
        sx127x_write_reg(sx127x, REG_DETECTION_OPTIMIZE, 0xc5);
        sx127x_write_reg(sx127x, REG_DETECTION_THRESHOLD, 0x0c);
    }
    else
    {
        sx127x_write_reg(sx127x, REG_DETECTION_OPTIMIZE, 0xc3);
        sx127x_write_reg(sx127x, REG_DETECTION_THRESHOLD, 0x0a);
    }
    sx127x_write_reg(sx127x, REG_MODEM_CONFIG_2, (sx127x_read_reg(sx127x, REG_MODEM_CONFIG_2) & 0x0f) | ((sf << 4) & 0xf0));
    sx127x->state.sf = sf;
}

void sx127x_set_lora_signal_bw(sx127x_t *sx127x, sx127x_lora_signal_bw_e sbw)
{
    sx127x_prepare_write(sx127x);

    if (sbw < SX127X_LORA_SIGNAL_BW_7_8)
    {
        sbw = SX127X_LORA_SIGNAL_BW_7_8;
    }
    else if (sbw > SX127X_LORA_SIGNAL_BW_500)
    {
        sbw = SX127X_LORA_SIGNAL_BW_500;
    }
    uint8_t reg = sx127x_read_reg(sx127x, REG_MODEM_CONFIG_1);
    reg = (reg & 0x0f) | (sbw << 4);
    sx127x_write_reg(sx127x, REG_MODEM_CONFIG_1, reg);
    sx127x->state.signal_bw = sbw;
}

void sx127x_set_lora_coding_rate(sx127x_t *sx127x, sx127x_lora_coding_rate_e rate)
{
    sx127x_prepare_write(sx127x);

    if (rate < SX127X_LORA_CODING_RATE_4_5)
    {
        rate = SX127X_LORA_CODING_RATE_4_5;
    }
    else if (rate > SX127X_LORA_CODING_RATE_4_8)
    {
        rate = SX127X_LORA_CODING_RATE_4_8;
    }

    uint8_t reg = sx127x_read_reg(sx127x, REG_MODEM_CONFIG_1);
    reg = (reg & 0xf1) | (rate << 1);
    sx127x_write_reg(sx127x, REG_MODEM_CONFIG_1, reg);
}

void sx127x_set_lora_preamble_length(sx127x_t *sx127x, long length)
{
    sx127x_prepare_write(sx127x);

    sx127x_write_reg(sx127x, REG_PREAMBLE_MSB, (uint8_t)(length >> 8));
    sx127x_write_reg(sx127x, REG_PREAMBLE_LSB, (uint8_t)(length >> 0));
}

void sx127x_set_lora_crc(sx127x_t *sx127x, bool crc)
{
    sx127x_prepare_write(sx127x);

    uint8_t reg = sx127x_read_reg(sx127x, REG_MODEM_CONFIG_2);
    if (crc)
    {
        reg |= 0x04;
    }
    else
    {
        reg &= 0xfb;
    }
    sx127x_write_reg(sx127x, REG_MODEM_CONFIG_2, reg);
}

void sx127x_set_lora_header_mode(sx127x_t *sx127x, sx127x_lora_header_e mode)
{
    uint8_t reg = sx127x_read_reg(sx127x, REG_MODEM_CONFIG_1);
    switch (mode)
    {
    case SX127X_LORA_HEADER_IMPLICIT:
        reg |= 0x01;
        break;
    case SX127X_LORA_HEADER_EXPLICIT:
        reg &= 0xfe;
        break;
    }
    sx127x_write_reg(sx127x, REG_MODEM_CONFIG_1, reg);
}

void sx127x_set_lora_sync_word(sx127x_t *sx127x, uint8_t sw)
{
    if (sw == 0)
    {
        // Sync word at zero won't work. See page 68 of the datasheet
        sw = 1;
    }
    else if (sw == 0x34)
    {
        // 0x34 is reserved for LoRaWAN
        sw = 0x35;
    }
    sx127x_write_reg(sx127x, REG_SYNC_WORD, sw);
}

int sx127x_lora_rx_sensitivity(sx127x_t *sx127x)
{
    // We just list the senstivities for BW500 with shared RFIO
    // Non-shared RFIO has +3db
    if (sx127x->state.signal_bw == SX127X_LORA_SIGNAL_BW_500)
    {
        switch (sx127x->state.sf)
        {
        case 6:
            return -109;
        case 7:
            return -114;
        case 8:
            return -117;
        case 9:
            return -120;
        case 10:
            return -123;
        case 11:
            return -125; // -125.5db actually
        case 12:
            return -128;
        }
    }
    // Fallback
    return sx127x_lora_min_rssi(sx127x) + 40;
}

int sx127x_lora_min_rssi(sx127x_t *sx127x)
{
    // Page 87, 5.5.5.
    if (sx127x->state.freq > 700)
    {
        // Using (HF) 862-1020MHz (779-960MHz*)
        return -157;
    }
    // Using (LF) 410-525 (*480)MHz or (LF) 137-175 (*160)MHz
    return -164;
}

// #pragma endregion
