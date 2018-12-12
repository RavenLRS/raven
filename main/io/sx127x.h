#pragma once

#include <stddef.h>
#include <stdint.h>

#include <hal/gpio.h>
#include <hal/spi.h>

#include "air/air_radio.h"

#include "util/time.h"

#define SX127X_MAX_PKT_LENGTH 255
#define SX127X_SNR_SCALE 4
#define SX127X_SYNC_WORD_DEFAULT -1

typedef enum
{
    SX127X_LORA_HEADER_IMPLICIT,
    SX127X_LORA_HEADER_EXPLICIT,
} sx127x_lora_header_e;

typedef enum
{
    SX127X_OUTPUT_RFO = 1,
    SX127X_OUTPUT_PA_BOOST,
} sx127x_output_type_e;

typedef enum
{
    SX127X_OP_MODE_FSK = 1,
    SX127X_OP_MODE_LORA = 2,
} sx127x_op_mode_e;

typedef enum
{
    SX127X_LORA_SIGNAL_BW_7_8 = 0,
    SX127X_LORA_SIGNAL_BW_10_4 = 1,
    SX127X_LORA_SIGNAL_BW_15_6 = 2,
    SX127X_LORA_SIGNAL_BW_20_8 = 3,
    SX127X_LORA_SIGNAL_BW_31_25 = 4,
    SX127X_LORA_SIGNAL_BW_41_7 = 5,
    SX127X_LORA_SIGNAL_BW_62_5 = 6,
    SX127X_LORA_SIGNAL_BW_250 = 8, // Not supported on lower band (169MHz)
    SX127X_LORA_SIGNAL_BW_500 = 9, // Not supported on lower band (169MHz)
} sx127x_lora_signal_bw_e;

typedef enum
{
    SX127X_LORA_CODING_RATE_4_5 = 1,
    SX127X_LORA_CODING_RATE_4_6 = 2,
    SX127X_LORA_CODING_RATE_4_7 = 3,
    SX127X_LORA_CODING_RATE_4_8 = 4,
} sx127x_lora_coding_rate_e;

#if 0
// See page 82, table 32. Frequencies marked * are for SX1279
typedef enum {
    LORA_BAND_1 = 1, // (HF) 862-1020MHz (779-960MHz*) - SX1276/77/79
    LORA_BAND_2,     // (LF) 410-525 (*480) MHz - SX1276/77/78/79
    LORA_BAND_3,     // (LF) 137-175 (*160)MHz - SX1276/77/78/79
} lora_band_e;
#endif

typedef struct sx127x_s
{
    const hal_spi_bus_t spi_bus;
    const hal_gpio_t miso;
    const hal_gpio_t mosi;
    const hal_gpio_t sck;
    const hal_gpio_t cs;
    const hal_gpio_t rst;
    const hal_gpio_t dio0;
    const hal_gpio_t txen;
    const hal_gpio_t rxen;
    const sx127x_output_type_e output_type;
    struct
    {
        hal_spi_device_handle_t spi;
        sx127x_op_mode_e op_mode;
        uint8_t mode;
        int16_t sync_word;
        struct
        {
            unsigned long freq;
            uint8_t payload_length;
            unsigned rx_bandwidth;
        } fsk;
        struct
        {
            unsigned long freq;
            uint8_t payload_length;
            uint8_t ppm_correction;
            sx127x_lora_signal_bw_e signal_bw;
            uint8_t bw_workaround;
            int sf;
        } lora;
        bool rx_done;
        bool tx_done;
        int dio0_trigger;
        void *callback;
        void *callback_data;
    } state;
} sx127x_t;

void sx127x_init(sx127x_t *sx127x);

void sx127x_set_op_mode(sx127x_t *sx127x, sx127x_op_mode_e op_mode);

void sx127x_set_tx_power(sx127x_t *sx127x, int dBm);
// freq is in Hz
void sx127x_set_frequency(sx127x_t *sx127x, unsigned long freq, int error);
// Should be called with center-ish frequency
void sx127x_calibrate(sx127x_t *sx127x, unsigned long freq);

void sx127x_set_payload_size(sx127x_t *sx127x, uint8_t size);
void sx127x_set_sync_word(sx127x_t *sx127x, int16_t sw);

void sx127x_send(sx127x_t *sx127x, const void *buf, size_t size);
size_t sx127x_read(sx127x_t *sx127x, void *buf, size_t size);
void sx127x_enable_continous_rx(sx127x_t *sx127x);
bool sx127x_is_tx_done(sx127x_t *sx127x);
bool sx127x_is_rx_done(sx127x_t *sx127x);
bool sx127x_is_rx_in_progress(sx127x_t *sx127x);

void sx127x_set_callback(sx127x_t *sx127x, air_radio_callback_t callback, void *data);

int sx127x_frequency_error(sx127x_t *sx127x);

int sx127x_rx_sensitivity(sx127x_t *sx127x);
// SNR is multiplied by 4
int sx127x_rssi(sx127x_t *sx127x, int *snr, int *lq);

void sx127x_idle(sx127x_t *sx127x);
void sx127x_sleep(sx127x_t *sx127x);

void sx127x_shutdown(sx127x_t *sx127x);

// FSK specific functions
void sx127x_set_fsk_fdev(sx127x_t *sx127x, unsigned hz);
void sx127x_set_fsk_bitrate(sx127x_t *sx127x, unsigned long bps);
void sx127x_set_fsk_rx_bandwidth(sx127x_t *sx127x, unsigned hz);
void sx127x_set_fsk_rx_afc_bandwidth(sx127x_t *sx127x, unsigned hz);
void sx127x_set_fsk_preamble_length(sx127x_t *sx127x, unsigned length);

// LoRa specific functions
void sx127x_set_lora_spreading_factor(sx127x_t *sx127x, int sf);
void sx127x_set_lora_signal_bw(sx127x_t *sx127x, sx127x_lora_signal_bw_e sbw);
void sx127x_set_lora_coding_rate(sx127x_t *sx127x, sx127x_lora_coding_rate_e rate);
void sx127x_set_lora_preamble_length(sx127x_t *sx127x, long length);
void sx127x_set_lora_crc(sx127x_t *sx127x, bool crc);
void sx127x_set_lora_header_mode(sx127x_t *sx127x, sx127x_lora_header_e mode);
int sx127x_lora_min_rssi(sx127x_t *sx127x);
