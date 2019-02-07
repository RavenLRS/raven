#include "target.h"

#include "air/air.h"

#include "io/sx127x.h"

#include "rc/telemetry.h"

#include "util/macros.h"

#include "air_radio_sx127x.h"

#if defined(USE_RADIO_SX127X)

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

void air_radio_calibrate(air_radio_t *radio, unsigned long freq)
{
    sx127x_calibrate(&radio->sx127x, freq);
}

int air_radio_frequency_error(air_radio_t *radio)
{
    return sx127x_frequency_error(&radio->sx127x);
}

void air_radio_set_sync_word(air_radio_t *radio, uint8_t word)
{
    sx127x_set_sync_word(&radio->sx127x, word);
}

void air_radio_start_rx(air_radio_t *radio)
{
    sx127x_enable_continous_rx(&radio->sx127x);
}

static void air_radio_sx127x_set_lora_mode_parameters(air_radio_t *radio)
{
    sx127x_set_op_mode(&radio->sx127x, SX127X_OP_MODE_LORA);
    sx127x_set_lora_signal_bw(&radio->sx127x, SX127X_LORA_SIGNAL_BW_500);
    sx127x_set_lora_header_mode(&radio->sx127x, SX127X_LORA_HEADER_IMPLICIT);
    sx127x_set_lora_crc(&radio->sx127x, false);
}

bool air_radio_should_switch_to_faster_mode(air_radio_t *radio, air_mode_e current, air_mode_e faster, int telemetry_id, telemetry_t *t)
{
    UNUSED(radio);

    if (telemetry_id == TELEMETRY_ID_RX_SNR)
    {
        // For switching up, we require an SNR of 4dB per mode. This only affects
        // the LoRa modes, since the only FSK mode is the fastest
        int8_t val = telemetry_get_i8(t, telemetry_id);
        return val >= 4 * (current - faster) * TELEMETRY_SNR_MULTIPLIER;
    }
    return false;
}

bool air_radio_should_switch_to_longer_mode(air_radio_t *radio, air_mode_e current, air_mode_e longer, int telemetry_id, telemetry_t *t)
{
    UNUSED(radio);
    UNUSED(longer);

    if (telemetry_id == TELEMETRY_ID_RX_SNR)
    {
        // For switching down in FSK mode, we use an SNR of 5dB. From the
        // LoRa modes, we switch down at 1.5dB
        int8_t val = telemetry_get_i8(t, telemetry_id);
        int threshold;
        if (current == AIR_MODE_1)
        {
            threshold = 5 * TELEMETRY_SNR_MULTIPLIER;
        }
        else
        {
            threshold = 1.5f * TELEMETRY_SNR_MULTIPLIER;
        }
        return val <= threshold;
    }
    return false;
}

unsigned air_radio_confirmations_required_for_switching_modes(air_radio_t *radio, air_mode_e current, air_mode_e to)
{
    UNUSED(radio);
    UNUSED(to);

    // Use 4 for MODE_5, 8 for MODE_4, ... up to a maximum of 15
    return MIN(15, 4 * ((AIR_MODE_LONGEST + 1) - current));
}

void air_radio_set_mode(air_radio_t *radio, air_mode_e mode)
{
    sx127x_sleep(&radio->sx127x);

    switch (mode)
    {
    case AIR_MODE_1:
        // Datasheet page 47, 4.2.2.1
        // FDEV + (BR / 2) <= 250000
        // Datasheet page 48, 4.2.3.1
        // 0.5 <= (2 * FDEV) / BR <= 10
        // Datasheet page 88, 5.5.6
        // BR < 2 x RxBw
        // Where:
        //  FDEV = Frequency deviation in Hz
        //  BR = Bitrate in bps
        //  RxBw = Receiver bandwidth in Hz
        sx127x_set_op_mode(&radio->sx127x, SX127X_OP_MODE_FSK);
        sx127x_set_fsk_fdev(&radio->sx127x, 125000);
        sx127x_set_fsk_bitrate(&radio->sx127x, 200000);
        sx127x_set_fsk_rx_bandwidth(&radio->sx127x, 250000);
        sx127x_set_fsk_rx_afc_bandwidth(&radio->sx127x, 250000);
        sx127x_set_fsk_preamble_length(&radio->sx127x, 5);
        break;
    case AIR_MODE_2:
        air_radio_sx127x_set_lora_mode_parameters(radio);
        sx127x_set_lora_preamble_length(&radio->sx127x, 6);
        sx127x_set_lora_spreading_factor(&radio->sx127x, 7);
        sx127x_set_lora_coding_rate(&radio->sx127x, SX127X_LORA_CODING_RATE_4_6);
        break;
    case AIR_MODE_3:
        air_radio_sx127x_set_lora_mode_parameters(radio);
        sx127x_set_lora_preamble_length(&radio->sx127x, 6);
        sx127x_set_lora_spreading_factor(&radio->sx127x, 8);
        sx127x_set_lora_coding_rate(&radio->sx127x, SX127X_LORA_CODING_RATE_4_6);
        break;
    case AIR_MODE_4:
        air_radio_sx127x_set_lora_mode_parameters(radio);
        sx127x_set_lora_preamble_length(&radio->sx127x, 6);
        sx127x_set_lora_spreading_factor(&radio->sx127x, 9);
        sx127x_set_lora_coding_rate(&radio->sx127x, SX127X_LORA_CODING_RATE_4_6);
        break;
    case AIR_MODE_5:
        air_radio_sx127x_set_lora_mode_parameters(radio);
        sx127x_set_lora_preamble_length(&radio->sx127x, 6);
        sx127x_set_lora_spreading_factor(&radio->sx127x, 10);
        sx127x_set_lora_coding_rate(&radio->sx127x, SX127X_LORA_CODING_RATE_4_8);
        break;
    }
}

void air_radio_set_bind_mode(air_radio_t *radio)
{
    // Same as fast parameters as mode 2
    air_radio_set_mode(radio, AIR_MODE_2);
    sx127x_set_tx_power(&radio->sx127x, 1);
    sx127x_set_sync_word(&radio->sx127x, SX127X_SYNC_WORD_DEFAULT);
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

bool air_radio_is_rx_in_progress(air_radio_t *radio)
{
    return sx127x_is_rx_in_progress(&radio->sx127x);
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

time_micros_t air_radio_cycle_time(air_radio_t *radio, air_mode_e mode)
{
    UNUSED(radio);

    switch (mode)
    {
    case AIR_MODE_1:
        return MILLIS_TO_MICROS(6.666);
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

time_micros_t air_radio_tx_failsafe_interval(air_radio_t *radio, air_mode_e mode)
{
    return air_radio_rx_failsafe_interval(radio, mode);
}

time_micros_t air_radio_rx_failsafe_interval(air_radio_t *radio, air_mode_e mode)
{
    UNUSED(radio);

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

#endif
