#include "target.h"

#include "air/air.h"

#include "rc/telemetry.h"

#include "air_radio_sx127x.h"

#if defined(USE_RADIO_FAKE)

void air_radio_init(air_radio_t *radio)
{
}

void air_radio_set_tx_power(air_radio_t *radio, int dBm)
{
}

void air_radio_set_frequency(air_radio_t *radio, unsigned long freq, int error)
{
}

void air_radio_calibrate(air_radio_t *radio, unsigned long freq)
{
}

int air_radio_frequency_error(air_radio_t *radio)
{
    return 0;
}

void air_radio_set_sync_word(air_radio_t *radio, uint8_t word)
{
}

void air_radio_start_rx(air_radio_t *radio)
{
}

bool air_radio_should_switch_to_faster_mode(air_radio_t *radio, air_mode_e current, air_mode_e faster, int telemetry_id, telemetry_t *t)
{
    return false;
}

bool air_radio_should_switch_to_longer_mode(air_radio_t *radio, air_mode_e current, air_mode_e longer, int telemetry_id, telemetry_t *t)
{
    return true;
}

unsigned air_radio_confirmations_required_for_switching_modes(air_radio_t *radio, air_mode_e current, air_mode_e to)
{
    return 1;
}

void air_radio_set_mode(air_radio_t *radio, air_mode_e mode)
{
}

void air_radio_set_bind_mode(air_radio_t *radio)
{
}

void air_radio_set_powertest_mode(air_radio_t *radio)
{
}

bool air_radio_is_tx_done(air_radio_t *radio)
{
    return true;
}

bool air_radio_is_rx_done(air_radio_t *radio)
{
    return true;
}

bool air_radio_is_rx_in_progress(air_radio_t *radio)
{
    return false;
}

void air_radio_set_payload_size(air_radio_t *radio, size_t size)
{
}

size_t air_radio_read(air_radio_t *radio, void *buf, size_t size)
{
    return size;
}

void air_radio_send(air_radio_t *radio, const void *buf, size_t size)
{
}

int air_radio_rssi(air_radio_t *radio, int *snr, int *lq)
{
    return 0;
}

void air_radio_set_callback(air_radio_t *radio, air_radio_callback_t callback, void *callback_data)
{
}

void air_radio_sleep(air_radio_t *radio)
{
}

void air_radio_shutdown(air_radio_t *radio)
{
}

time_micros_t air_radio_cycle_time(air_radio_t *radio, air_mode_e mode)
{
    UNUSED(radio);

    return MILLIS_TO_MICROS(500);
}

time_micros_t air_radio_tx_failsafe_interval(air_radio_t *radio, air_mode_e mode)
{
    return air_radio_rx_failsafe_interval(radio, mode);
}

time_micros_t air_radio_rx_failsafe_interval(air_radio_t *radio, air_mode_e mode)
{
    return MILLIS_TO_MICROS(100000);
}

#endif
