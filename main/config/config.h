#pragma once

#include <stdint.h>

#include "air/air.h"

#define CONFIG_MAX_PAIRED_RX 32 // Max RX paired to a TX

typedef struct air_pairing_s air_pairing_t;
typedef struct air_addr_s air_addr_t;

typedef enum {
    RC_MODE_TX,
    RC_MODE_RX,
} rc_mode_e;

typedef enum {
    TX_INPUT_CRSF,
    TX_INPUT_FAKE,
    TX_INPUT_FIRST = TX_INPUT_CRSF,
#if defined(USE_TX_FAKE_INPUT)
    TX_INPUT_LAST = TX_INPUT_FAKE,
#else
    TX_INPUT_LAST = TX_INPUT_CRSF,
#endif
} tx_input_type_e;

typedef enum {
    TX_RF_POWER_AUTO = 0,
    TX_RF_POWER_1mw,
    TX_RF_POWER_10mw,
    TX_RF_POWER_25mw,
    TX_RF_POWER_50mw,
    TX_RF_POWER_100mw,

    TX_RF_POWER_FIRST = TX_RF_POWER_AUTO,
    TX_RF_POWER_LAST = TX_RF_POWER_100mw,
    TX_RF_POWER_DEFAULT = TX_RF_POWER_AUTO,
} tx_rf_power_e;

typedef enum {
    RX_OUTPUT_SBUS_SPORT,
    RX_OUTPUT_MSP,
    RX_OUTPUT_CRSF,
    RX_OUTPUT_FPORT,

    RX_OUTPUT_FIRST = RX_OUTPUT_SBUS_SPORT,
    RX_OUTPUT_LAST = RX_OUTPUT_FPORT,
} rx_output_type_e;

void config_init(void);

#if defined(USE_TX_SUPPORT) && defined(USE_RX_SUPPORT)
rc_mode_e config_get_rc_mode(void);
#elif defined(USE_TX_SUPPORT)
#define config_get_rc_mode() RC_MODE_TX
#elif defined(USE_RX_SUPPORT)
#define config_get_rc_mode() RC_MODE_RX
#else
#error USE_TX_SUPPORT and USE_RX_SUPPORT both undefined
#endif

bool config_get_paired_rx(air_pairing_t *pairing, air_addr_t *addr);
void config_add_paired_rx(const air_pairing_t *pairing);
bool config_get_paired_rx_at(air_pairing_t *pairing, int idx);
bool config_remove_paired_rx_at(int idx);

bool config_get_paired_tx(air_pairing_t *pairing);
void config_set_paired_tx(const air_pairing_t *pairing);

bool config_get_air_name(char *buf, size_t size, const air_addr_t *addr);
bool config_set_air_name(const air_addr_t *addr, const char *name);
bool config_get_air_info(air_info_t *info, const air_addr_t *addr);
bool config_set_air_info(const air_addr_t *addr, const air_info_t *info);

// Used for storing keys for talking to other devices, including TX/RX/GS
// and other RC chains using p2p.
bool config_get_pairing(air_pairing_t *pairing, air_addr_t *addr);

tx_input_type_e config_get_input_type(void);
rx_output_type_e config_get_output_type(void);

air_addr_t config_get_addr(void);
