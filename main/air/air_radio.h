#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "air/air_mode.h"

#include "util/time.h"

typedef struct air_radio_s air_radio_t;
typedef struct telemetry_s telemetry_t;

typedef enum
{
    AIR_RADIO_CALLBACK_REASON_TX_DONE,
    AIR_RADIO_CALLBACK_REASON_RX_DONE,
} air_radio_callback_reason_e;

void air_radio_init(air_radio_t *radio);
void air_radio_set_tx_power(air_radio_t *radio, int dBm);
void air_radio_set_frequency(air_radio_t *radio, unsigned long freq, int error);
void air_radio_calibrate(air_radio_t *radio, unsigned long freq);
int air_radio_frequency_error(air_radio_t *radio);

void air_radio_set_sync_word(air_radio_t *radio, uint8_t word);

void air_radio_start_rx(air_radio_t *radio);

bool air_radio_should_switch_to_faster_mode(air_radio_t *radio, air_mode_e current, air_mode_e faster, int telemetry_id, telemetry_t *t);
bool air_radio_should_switch_to_longer_mode(air_radio_t *radio, air_mode_e current, air_mode_e longer, int telemetry_id, telemetry_t *t);
unsigned air_radio_confirmations_required_for_switching_modes(air_radio_t *radio, air_mode_e current, air_mode_e to);
void air_radio_set_mode(air_radio_t *radio, air_mode_e mode);

void air_radio_set_bind_mode(air_radio_t *radio);
void air_radio_set_powertest_mode(air_radio_t *radio);

bool air_radio_is_tx_done(air_radio_t *radio);
bool air_radio_is_rx_done(air_radio_t *radio);
bool air_radio_is_rx_in_progress(air_radio_t *radio);

void air_radio_set_payload_size(air_radio_t *radio, size_t size);
size_t air_radio_read(air_radio_t *radio, void *buf, size_t size);
void air_radio_send(air_radio_t *radio, const void *buf, size_t size);

int air_radio_rssi(air_radio_t *radio, int *snr, int *lq);

typedef void (*air_radio_callback_t)(air_radio_t *radio, air_radio_callback_reason_e reason, void *data);
void air_radio_set_callback(air_radio_t *radio, air_radio_callback_t callback, void *callback_data);

void air_radio_sleep(air_radio_t *radio);
void air_radio_shutdown(air_radio_t *radio);

time_micros_t air_radio_cycle_time(air_radio_t *radio, air_mode_e mode);
time_micros_t air_radio_tx_failsafe_interval(air_radio_t *radio, air_mode_e mode);
time_micros_t air_radio_rx_failsafe_interval(air_radio_t *radio, air_mode_e mode);
