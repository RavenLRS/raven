
#include "air/air.h"

#include "io/lora.h"

#include "air_lora.h"

void air_lora_set_parameters(struct lora_s *lora, air_lora_mode_e mode)
{
    lora_idle(lora);

    lora_set_signal_bw(lora, LORA_SIGNAL_BW_500);
    lora_set_header_mode(lora, LORA_HEADER_IMPLICIT);
    lora_set_crc(lora, false);

    switch (mode)
    {
    case AIR_LORA_MODE_1:
        // We reduce coding rate in this mode to be as fast
        // as possible. Intended for short range and fast
        // update rate.
        lora_set_preamble_length(lora, 8);
        lora_set_spreading_factor(lora, 6);
        lora_set_coding_rate(lora, LORA_CODING_RATE_4_5);
        break;
    case AIR_LORA_MODE_2:
        lora_set_preamble_length(lora, 6);
        lora_set_spreading_factor(lora, 7);
        lora_set_coding_rate(lora, LORA_CODING_RATE_4_6);
        break;
    case AIR_LORA_MODE_3:
        lora_set_preamble_length(lora, 6);
        lora_set_spreading_factor(lora, 8);
        lora_set_coding_rate(lora, LORA_CODING_RATE_4_6);
        break;
    case AIR_LORA_MODE_4:
        lora_set_preamble_length(lora, 6);
        lora_set_spreading_factor(lora, 9);
        lora_set_coding_rate(lora, LORA_CODING_RATE_4_6);
        break;
    case AIR_LORA_MODE_5:
        lora_set_preamble_length(lora, 6);
        lora_set_spreading_factor(lora, 10);
        lora_set_coding_rate(lora, LORA_CODING_RATE_4_8);
        break;
    }
}

time_micros_t air_lora_full_cycle_time(air_lora_mode_e mode)
{
    switch (mode)
    {
    case AIR_LORA_MODE_1:
        return MILLIS_TO_MICROS(10);
    case AIR_LORA_MODE_2:
        return MILLIS_TO_MICROS(20);
    case AIR_LORA_MODE_3:
        return MILLIS_TO_MICROS(33);
    case AIR_LORA_MODE_4:
        return MILLIS_TO_MICROS(55);
    case AIR_LORA_MODE_5:
        return MILLIS_TO_MICROS(115);
    }
    UNREACHABLE();
    return 0;
}

time_micros_t air_lora_uplink_cycle_time(air_lora_mode_e mode)
{
    switch (mode)
    {
    case AIR_LORA_MODE_1:
        return MILLIS_TO_MICROS(4.5f);
    case AIR_LORA_MODE_2:
        return MILLIS_TO_MICROS(20);
    case AIR_LORA_MODE_3:
        return MILLIS_TO_MICROS(31);
    case AIR_LORA_MODE_4:
        return MILLIS_TO_MICROS(75);
    case AIR_LORA_MODE_5:
        return MILLIS_TO_MICROS(115);
    }
    UNREACHABLE();
    return 0;
}

bool air_lora_cycle_is_full(air_lora_mode_e mode, unsigned seq)
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

time_micros_t air_lora_tx_failsafe_interval(air_lora_mode_e mode)
{
    return air_lora_rx_failsafe_interval(mode);
}

time_micros_t air_lora_rx_failsafe_interval(air_lora_mode_e mode)
{
    switch (mode)
    {
    case AIR_LORA_MODE_1:
        return MILLIS_TO_MICROS(250);
    case AIR_LORA_MODE_2:
        return MILLIS_TO_MICROS(300);
    case AIR_LORA_MODE_3:
        return MILLIS_TO_MICROS(400);
    case AIR_LORA_MODE_4:
        return MILLIS_TO_MICROS(500);
    case AIR_LORA_MODE_5:
        return MILLIS_TO_MICROS(700);
    }
    UNREACHABLE();
    return 0;
}

void air_lora_set_parameters_bind(lora_t *lora, air_lora_band_e band)
{
    // TODO: Low power TX

    lora_set_frequency(lora, air_lora_band_frequency(band));
    lora_set_sync_word(lora, LORA_DEFAULT_SYNC_WORD);
    // Same as fast parameters as short range mode
    air_lora_set_parameters(lora, AIR_LORA_MODE_FASTEST);
    lora_set_payload_size(lora, sizeof(air_bind_packet_t));
}