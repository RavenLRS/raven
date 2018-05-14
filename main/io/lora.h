#pragma once

#include <stddef.h>
#include <stdint.h>

#include <driver/gpio.h>
#include <driver/spi_master.h>

#include "time.h"

#define LORA_MAX_PKT_LENGTH 255

#define LORA_DEFAULT_SYNC_WORD 0x12 // 6.4 LoRa register mode map, RegSyncWord

typedef enum
{
    LORA_HEADER_IMPLICIT,
    LORA_HEADER_EXPLICIT,
} lora_header_e;

typedef enum
{
    LORA_OUTPUT_RFO = 1,
    LORA_OUTPUT_PA_BOOST,
} lora_output_type_e;

typedef enum
{
    LORA_SIGNAL_BW_7_8 = 0,
    LORA_SIGNAL_BW_10_4 = 1,
    LORA_SIGNAL_BW_15_6 = 2,
    LORA_SIGNAL_BW_20_8 = 3,
    LORA_SIGNAL_BW_31_25 = 4,
    LORA_SIGNAL_BW_41_7 = 5,
    LORA_SIGNAL_BW_62_5 = 6,
    LORA_SIGNAL_BW_250 = 8, // Not supported on lower band (169MHz)
    LORA_SIGNAL_BW_500 = 9, // Not supported on lower band (169MHz)
} lora_signal_bw_e;

typedef enum
{
    LORA_CODING_RATE_4_5 = 1,
    LORA_CODING_RATE_4_6 = 2,
    LORA_CODING_RATE_4_7 = 3,
    LORA_CODING_RATE_4_8 = 4,
} lora_coding_rate_e;

#if 0
// See page 82, table 32. Frequencies marked * are for SX1279
typedef enum {
    LORA_BAND_1 = 1, // (HF) 862-1020MHz (779-960MHz*) - SX1276/77/79
    LORA_BAND_2,     // (LF) 410-525 (*480) MHz - SX1276/77/78/79
    LORA_BAND_3,     // (LF) 137-175 (*160)MHz - SX1276/77/78/79
} lora_band_e;
#endif

typedef enum
{
    LORA_CALLBACK_REASON_TX_DONE,
    LORA_CALLBACK_REASON_RX_DONE,
} lora_callback_reason_e;

typedef struct lora_s
{
    const gpio_num_t miso;
    const gpio_num_t mosi;
    const gpio_num_t sck;
    const gpio_num_t cs;
    const gpio_num_t rst;
    const gpio_num_t dio0;
    const lora_output_type_e output_type;
    struct
    {
        spi_device_handle_t spi;
        unsigned long freq;
        uint8_t mode;
        uint8_t payload_size;
        bool rx_done;
        bool tx_done;
        int dio0_trigger;
        void *callback;
        void *callback_data;
        unsigned tx_start;
        unsigned tx_end;
        lora_signal_bw_e signal_bw;
        int sf;
    } state;
} lora_t;

void lora_init(lora_t *lora);

void lora_set_tx_power(lora_t *lora, int dBm);
// freq is in Hz
void lora_set_frequency(lora_t *lora, unsigned long freq);
void lora_set_spreading_factor(lora_t *lora, int sf);
void lora_set_signal_bw(lora_t *lora, lora_signal_bw_e sbw);
void lora_set_coding_rate(lora_t *lora, lora_coding_rate_e rate);
void lora_set_preamble_length(lora_t *lora, long length);
void lora_set_crc(lora_t *lora, bool crc);
void lora_set_header_mode(lora_t *lora, lora_header_e mode);
void lora_set_sync_word(lora_t *lora, uint8_t sw);
void lora_set_payload_size(lora_t *lora, uint8_t size);

void lora_send(lora_t *lora, const void *buf, size_t size);
size_t lora_wait(lora_t *lora, size_t size);
size_t lora_read(lora_t *lora, void *buf, size_t size);
void lora_enable_continous_rx(lora_t *lora);
bool lora_is_tx_done(lora_t *lora);
bool lora_is_rx_done(lora_t *lora);

typedef void (*lora_callback_t)(lora_t *lora, lora_callback_reason_e reason, void *data);
void lora_set_callback(lora_t *lora, lora_callback_t callback, void *data);

int lora_min_rssi(lora_t *lora);
// SNR is multiplied by 4
int lora_rssi(lora_t *lora, int *snr, int *lq);

void lora_idle(lora_t *lora);
void lora_sleep(lora_t *lora);

void lora_shutdown(lora_t *lora);
