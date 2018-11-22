#pragma once

#include <stdint.h>

#include "target.h"

#include "air/air.h"
#include "air/air_band.h"
#include "air/air_mode.h"

#include "rc/rc_data.h"

#define CONFIG_MAX_PAIRED_RX 32 // Max RX paired to a TX

typedef struct air_pairing_s air_pairing_t;
typedef struct air_addr_s air_addr_t;

typedef enum
{
    RC_MODE_TX,
    RC_MODE_RX,
} rc_mode_e;

typedef enum
{
    TX_INPUT_CRSF,
    TX_INPUT_IBUS,
    TX_INPUT_FAKE,
    TX_INPUT_FIRST = TX_INPUT_CRSF,
#if defined(CONFIG_RAVEN_FAKE_INPUT)
    TX_INPUT_LAST = TX_INPUT_FAKE,
#else
    TX_INPUT_LAST = TX_INPUT_CRSF,
#endif
} tx_input_type_e;

typedef enum
{
    RX_OUTPUT_MSP,
    RX_OUTPUT_CRSF,
    RX_OUTPUT_FPORT,
    RX_OUTPUT_SBUS_SPORT,
    RX_OUTPUT_NONE,

    RX_OUTPUT_FIRST = RX_OUTPUT_MSP,
    RX_OUTPUT_LAST = RX_OUTPUT_NONE,
} rx_output_type_e;

typedef enum
{
    RX_RSSI_CHANNEL_AUTO,
    RX_RSSI_CHANNEL_NONE,
    RX_RSSI_CHANNEL_1,
    RX_RSSI_CHANNEL_2,
    RX_RSSI_CHANNEL_3,
    RX_RSSI_CHANNEL_4,
    RX_RSSI_CHANNEL_5,
    RX_RSSI_CHANNEL_6,
    RX_RSSI_CHANNEL_7,
    RX_RSSI_CHANNEL_8,
    RX_RSSI_CHANNEL_9,
    RX_RSSI_CHANNEL_10,
    RX_RSSI_CHANNEL_11,
    RX_RSSI_CHANNEL_12,
#if RC_CHANNELS_NUM > 12
    RX_RSSI_CHANNEL_13,
    RX_RSSI_CHANNEL_14,
    RX_RSSI_CHANNEL_15,
    RX_RSSI_CHANNEL_16,
#if RC_CHANNELS_NUM > 16
    RX_RSSI_CHANNEL_17,
    RX_RSSI_CHANNEL_18,
#if RC_CHANNELS_NUM > 18
    RX_RSSI_CHANNEL_19,
    RX_RSSI_CHANNEL_20,
#endif
#endif
#endif
    RX_RSSI_CHANNEL_COUNT,
} rx_rssi_channel_e;

inline int rx_rssi_channel_index(rx_rssi_channel_e ch)
{
    return ch > RX_RSSI_CHANNEL_NONE ? ch - 2 : -1;
}

_Static_assert(RC_CHANNELS_NUM == 12 || RC_CHANNELS_NUM == 16 || RC_CHANNELS_NUM == 18 || RC_CHANNELS_NUM == 20, "Adjust rx_rssi_channel_e to support RC_CHANNELS_NUM");
_Static_assert(RX_RSSI_CHANNEL_COUNT == RC_CHANNELS_NUM + 2, "RX_RSSI_CHANNEL_COUNT is not valid for the number of channels");

// Used to store it as a setting, since settings need continuous
// values starting at zero.

typedef enum
{
    CONFIG_AIR_MODES_1_5,
    CONFIG_AIR_MODES_2_5,
    CONFIG_AIR_MODES_FIXED_1,
    CONFIG_AIR_MODES_FIXED_2,
    CONFIG_AIR_MODES_FIXED_3,
    CONFIG_AIR_MODES_FIXED_4,
    CONFIG_AIR_MODES_FIXED_5,

    CONFIG_AIR_MODES_COUNT,
} config_air_mode_e;

typedef enum
{
#if defined(USE_AIR_BAND_147)
    CONFIG_AIR_BAND_147,
#endif
#if defined(USE_AIR_BAND_169)
    CONFIG_AIR_BAND_169,
#endif
#if defined(USE_AIR_BAND_315)
    CONFIG_AIR_BAND_315,
#endif
#if defined(USE_AIR_BAND_433)
    CONFIG_AIR_BAND_433,
#endif
#if defined(USE_AIR_BAND_470)
    CONFIG_AIR_BAND_470,
#endif
#if defined(USE_AIR_BAND_868)
    CONFIG_AIR_BAND_868,
#endif
#if defined(USE_AIR_BAND_915)
    CONFIG_AIR_BAND_915,
#endif
    CONFIG_AIR_BAND_COUNT,
} config_air_band_e;

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

bool config_get_paired_rx(air_pairing_t *pairing, const air_addr_t *addr);
void config_add_paired_rx(const air_pairing_t *pairing);
bool config_get_paired_rx_at(air_pairing_t *pairing, int idx);
bool config_remove_paired_rx_at(int idx);

bool config_get_paired_tx(air_pairing_t *pairing);
void config_set_paired_tx(const air_pairing_t *pairing);

bool config_get_air_name(char *buf, size_t size, const air_addr_t *addr);
bool config_set_air_name(const air_addr_t *addr, const char *name);
bool config_get_air_info(air_info_t *info, air_band_e *band, const air_addr_t *addr);
// True true iff info was updated, if there were no changes it returns false
bool config_set_air_info(const air_addr_t *addr, const air_info_t *info, air_band_e band);

// Used for storing keys for talking to other devices, including TX/RX/GS
// and other RC chains using p2p.
bool config_get_pairing(air_pairing_t *pairing, const air_addr_t *addr);

tx_input_type_e config_get_input_type(void);
rx_output_type_e config_get_output_type(void);

air_addr_t config_get_addr(void);

air_band_e config_get_air_band(config_air_band_e band);
air_band_mask_t config_get_air_band_mask(void);
bool config_supports_air_band(air_band_e band);
air_supported_modes_e config_get_air_modes(config_air_mode_e modes);
