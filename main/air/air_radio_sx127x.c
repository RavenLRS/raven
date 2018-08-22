#include "air/air.h"

#include "io/sx127x.h"

#include "air_radio_sx127x.h"

void air_radio_init(air_radio_t *radio)
{
    sx127x_init(&radio->sx127x);
}

void air_radio_set_tx_power(air_radio_t *radio, int dBm)
{
    sx127x_set_tx_power(&radio->sx127x, dBm);
}

void air_radio_set_frequency(air_radio_t *radio, unsigned long freq, int error)
{
    sx127x_set_frequency(&radio->sx127x, freq, error);
}

int air_radio_frequency_error(air_radio_t *radio)
{
    return sx127x_frequency_error(&radio->sx127x);
}

void air_radio_set_sync_word(air_radio_t *radio, uint8_t word)
{
    sx127x_set_lora_sync_word(&radio->sx127x, word);
}

void air_radio_start_rx(air_radio_t *radio)
{
    sx127x_enable_continous_rx(&radio->sx127x);
}

void air_radio_set_mode(air_radio_t *radio, air_mode_e mode)
{
    sx127x_idle(&radio->sx127x);

    sx127x_set_lora_signal_bw(&radio->sx127x, SX127X_LORA_SIGNAL_BW_500);
    sx127x_set_lora_header_mode(&radio->sx127x, SX127X_LORA_HEADER_IMPLICIT);
    sx127x_set_lora_crc(&radio->sx127x, false);

    switch (mode)
    {
    case AIR_MODE_1:
        // We reduce coding rate in this mode to be as fast
        // as possible. Intended for short range and fast
        // update rate.
        sx127x_set_lora_preamble_length(&radio->sx127x, 8);
        sx127x_set_lora_spreading_factor(&radio->sx127x, 6);
        sx127x_set_lora_coding_rate(&radio->sx127x, SX127X_LORA_CODING_RATE_4_5);
        break;
    case AIR_MODE_2:
        sx127x_set_lora_preamble_length(&radio->sx127x, 6);
        sx127x_set_lora_spreading_factor(&radio->sx127x, 7);
        sx127x_set_lora_coding_rate(&radio->sx127x, SX127X_LORA_CODING_RATE_4_6);
        break;
    case AIR_MODE_3:
        sx127x_set_lora_preamble_length(&radio->sx127x, 6);
        sx127x_set_lora_spreading_factor(&radio->sx127x, 8);
        sx127x_set_lora_coding_rate(&radio->sx127x, SX127X_LORA_CODING_RATE_4_6);
        break;
    case AIR_MODE_4:
        sx127x_set_lora_preamble_length(&radio->sx127x, 6);
        sx127x_set_lora_spreading_factor(&radio->sx127x, 9);
        sx127x_set_lora_coding_rate(&radio->sx127x, SX127X_LORA_CODING_RATE_4_6);
        break;
    case AIR_MODE_5:
        sx127x_set_lora_preamble_length(&radio->sx127x, 6);
        sx127x_set_lora_spreading_factor(&radio->sx127x, 10);
        sx127x_set_lora_coding_rate(&radio->sx127x, SX127X_LORA_CODING_RATE_4_8);
        break;
    }
}

void air_radio_set_bind_mode(air_radio_t *radio)
{
    sx127x_set_tx_power(&radio->sx127x, 1);
    sx127x_set_lora_sync_word(&radio->sx127x, SX127X_DEFAULT_LORA_SYNC_WORD);
    // Same as fast parameters as short range mode
    air_radio_set_mode(radio, AIR_MODE_FASTEST);
    sx127x_set_payload_size(&radio->sx127x, sizeof(air_bind_packet_t));
}

void air_radio_set_powertest_mode(air_radio_t *radio)
{
    air_radio_set_mode(radio, AIR_MODE_LONGEST);
    sx127x_set_lora_spreading_factor(&radio->sx127x, 12);
    sx127x_set_lora_signal_bw(&radio->sx127x, SX127X_LORA_SIGNAL_BW_250);
}

bool air_radio_is_tx_done(air_radio_t *radio)
{
    return sx127x_is_tx_done(&radio->sx127x);
}

bool air_radio_is_rx_done(air_radio_t *radio)
{
    return sx127x_is_rx_done(&radio->sx127x);
}

void air_radio_set_payload_size(air_radio_t *radio, size_t size)
{
    sx127x_set_payload_size(&radio->sx127x, size);
}

size_t air_radio_read(air_radio_t *radio, void *buf, size_t size)
{
    return sx127x_read(&radio->sx127x, buf, size);
}

void air_radio_send(air_radio_t *radio, const void *buf, size_t size)
{
    sx127x_send(&radio->sx127x, buf, size);
}

int air_radio_rssi(air_radio_t *radio, int *snr, int *lq)
{
    return sx127x_rssi(&radio->sx127x, snr, lq);
}

void air_radio_set_callback(air_radio_t *radio, air_radio_callback_t callback, void *callback_data)
{
    sx127x_set_callback(&radio->sx127x, callback, callback_data);
}

void air_radio_sleep(air_radio_t *radio)
{
    sx127x_sleep(&radio->sx127x);
}

void air_radio_shutdown(air_radio_t *radio)
{
    sx127x_shutdown(&radio->sx127x);
}

time_micros_t air_radio_full_cycle_time(air_radio_t *radio, air_mode_e mode)
{
    switch (mode)
    {
    case AIR_MODE_1:
        return MILLIS_TO_MICROS(10);
    case AIR_MODE_2:
        return MILLIS_TO_MICROS(20);
    case AIR_MODE_3:
        return MILLIS_TO_MICROS(33);
    case AIR_MODE_4:
        return MILLIS_TO_MICROS(66);
    case AIR_MODE_5:
        return MILLIS_TO_MICROS(115);
    }
    UNREACHABLE();
    return 0;
}

time_micros_t air_radio_uplink_cycle_time(air_radio_t *radio, air_mode_e mode)
{
    switch (mode)
    {
    case AIR_MODE_1:
        return MILLIS_TO_MICROS(4.5f);
    case AIR_MODE_2:
        return MILLIS_TO_MICROS(20);
    case AIR_MODE_3:
        return MILLIS_TO_MICROS(31);
    case AIR_MODE_4:
        return MILLIS_TO_MICROS(75);
    case AIR_MODE_5:
        return MILLIS_TO_MICROS(115);
    }
    UNREACHABLE();
    return 0;
}

bool air_radio_cycle_is_full(air_radio_t *radio, air_mode_e mode, unsigned seq)
{
#if 0
    // TODO
    if (mode == AIR_LORA_MODE_1)
    {
        return seq % 6 == 0;
    }
#endif
    return true;
}

time_micros_t air_radio_tx_failsafe_interval(air_radio_t *radio, air_mode_e mode)
{
    return air_radio_rx_failsafe_interval(radio, mode);
}

time_micros_t air_radio_rx_failsafe_interval(air_radio_t *radio, air_mode_e mode)
{
    switch (mode)
    {
    case AIR_MODE_1:
        return MILLIS_TO_MICROS(250);
    case AIR_MODE_2:
        return MILLIS_TO_MICROS(300);
    case AIR_MODE_3:
        return MILLIS_TO_MICROS(400);
    case AIR_MODE_4:
        return MILLIS_TO_MICROS(500);
    case AIR_MODE_5:
        return MILLIS_TO_MICROS(700);
    }
    UNREACHABLE();
    return 0;
}
