#include <string.h>

#include "rc_data.h"

// There are downlink fields set by the air input, so we
// need to reset them when the input changes and keep them
// when the output changes.
static telemetry_downlink_id_e input_downlink_telemetry[] = {
    TELEMETRY_ID_RX_RSSI_ANT1,
    TELEMETRY_ID_RX_RSSI_ANT2,
    TELEMETRY_ID_RX_LINK_QUALITY,
    TELEMETRY_ID_RX_SNR,
    TELEMETRY_ID_RX_ACTIVE_ANT,
    TELEMETRY_ID_RX_RF_POWER,
};

void rc_data_reset_input(rc_data_t *data)
{
    memset(data->channels, 0, sizeof(data->channels));
    for (int ii = 0; ii < RC_CHANNELS_NUM; ii++)
    {
        data->channels[ii].value = RC_CHANNEL_CENTER_VALUE;
    }
    memset(data->telemetry_uplink, 0, sizeof(data->telemetry_uplink));
    for (int ii = 0; ii < ARRAY_COUNT(input_downlink_telemetry); ii++)
    {
        memset(&data->telemetry_downlink[TELEMETRY_DOWNLINK_GET_IDX(input_downlink_telemetry[ii])], 0, sizeof(data->telemetry_downlink[0]));
    }
    // Reset data states for fields updated by the input
    for (int ii = 0; ii < RC_CHANNELS_NUM; ii++)
    {
        data_state_init(&data->channels[ii].data_state);
    }
    for (int ii = 0; ii < ARRAY_COUNT(data->telemetry_uplink); ii++)
    {
        data_state_init(&data->telemetry_uplink[ii].data_state);
    }
    data->channels_num = RC_CHANNELS_NUM;
    data->ready = false;
#ifdef SETUP_FAKE_TELEMETRY
    time_ticks_t now = time_ticks_now();
    TELEMETRY_SET_I8(data, TELEMETRY_ID_TX_RSSI_ANT1, 73, now);
    TELEMETRY_SET_I8(data, TELEMETRY_ID_TX_RF_POWER, 20, now);
    TELEMETRY_SET_I8(data, TELEMETRY_ID_TX_LINK_QUALITY, 90, now);
    TELEMETRY_SET_I8(data, TELEMETRY_ID_TX_SNR, 30, now);

    TELEMETRY_SET_I8(data, TELEMETRY_ID_RX_RSSI_ANT1, 50, now);
    TELEMETRY_SET_I8(data, TELEMETRY_ID_RX_RSSI_ANT2, 52, now);
    TELEMETRY_SET_I8(data, TELEMETRY_ID_RX_LINK_QUALITY, 70, now);
    TELEMETRY_SET_I8(data, TELEMETRY_ID_RX_SNR, 30, now);
    TELEMETRY_SET_U8(data, TELEMETRY_ID_RX_ACTIVE_ANT, 1, now);
#endif
}

void rc_data_reset_output(rc_data_t *data)
{
    // input_downlink_telemetry fields are set by the input, so we need to keep them
    for (int ii = 0; ii < TELEMETRY_DOWNLINK_COUNT; ii++)
    {
        bool found = false;
        for (int jj = 0; jj < ARRAY_COUNT(input_downlink_telemetry); jj++)
        {
            if (ii == TELEMETRY_DOWNLINK_GET_IDX(input_downlink_telemetry[jj]))
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            memset(&data->telemetry_downlink[ii], 0, sizeof(data->telemetry_downlink[0]));
        }
    }
    // Reset data states for fields updated by the output
    for (int ii = 0; ii < ARRAY_COUNT(data->telemetry_downlink); ii++)
    {
        data_state_init(&data->telemetry_downlink[ii].data_state);
    }
#ifdef SETUP_FAKE_TELEMETRY
    time_ticks_t now = time_ticks_now();
    (void)TELEMETRY_SET_U16(data, TELEMETRY_ID_BAT_VOLTAGE, 14.7 * 100, now);
    (void)TELEMETRY_SET_U16(data, TELEMETRY_ID_AVG_CELL_VOLTAGE, 3.78 * 100, now);
    (void)TELEMETRY_SET_I16(data, TELEMETRY_ID_CURRENT, 12.3 * 100, now);
    (void)TELEMETRY_SET_I32(data, TELEMETRY_ID_CURRENT_DRAWN, 1234, now);
    (void)TELEMETRY_SET_U8(data, TELEMETRY_ID_BAT_REMAINING_P, 42, now);
    (void)TELEMETRY_SET_STR(data, TELEMETRY_ID_FLIGHT_MODE_NAME, "!ACRO", now);
    (void)TELEMETRY_SET_I16(data, TELEMETRY_ID_ATTITUDE_X, -165 * 100, now);
    (void)TELEMETRY_SET_I16(data, TELEMETRY_ID_ATTITUDE_Y, 180 * 100, now);
    (void)TELEMETRY_SET_I16(data, TELEMETRY_ID_ATTITUDE_Z, 5.47 * 100, now);
    (void)TELEMETRY_SET_I32(data, TELEMETRY_ID_GPS_LAT, 42.123456 * 10000000, now);
    (void)TELEMETRY_SET_I32(data, TELEMETRY_ID_GPS_LON, -5.123456 * 10000000, now);
    (void)TELEMETRY_SET_U16(data, TELEMETRY_ID_GPS_SPEED, 20 * 100, now);
    (void)TELEMETRY_SET_U16(data, TELEMETRY_ID_GPS_HEADING, 120 * 100, now);
    (void)TELEMETRY_SET_I32(data, TELEMETRY_ID_GPS_ALT, 107.05 * 100, now);
    (void)TELEMETRY_SET_U8(data, TELEMETRY_ID_GPS_NUM_SATS, 13, now);
#endif
}

void rc_data_update_channel(rc_data_t *data, unsigned ch, unsigned value, time_micros_t now)
{
    if (ch >= data->channels_num)
    {
        // An input protocol supports more channels that the
        // ones enabled at compile time. Stop here.
        return;
    }
    control_channel_t *channel = &data->channels[ch];
    value = value < RC_CHANNEL_MAX_VALUE ? value : RC_CHANNEL_MAX_VALUE;
    value = value > RC_CHANNEL_MIN_VALUE ? value : RC_CHANNEL_MIN_VALUE;
    bool changed = channel->value != value;
    channel->value = value;
    data_state_update(&channel->data_state, changed, now);
}

uint16_t rc_data_get_channel_value(const rc_data_t *data, unsigned ch)
{
    if (ch >= data->channels_num)
    {
        // An output protocol supports more channels that the
        // ones enabled at compile time. Return zero.
        return 0;
    }
    return data->channels[ch].value;
}

bool rc_data_is_ready(rc_data_t *data)
{
    if (!data->ready)
    {
        for (unsigned ii = 0; ii < data->channels_num; ii++)
        {
            control_channel_t *ch = &data->channels[ii];
            if (!data_state_has_value(&ch->data_state))
            {
                // Channel has no valid value yet
                return false;
            }
        }
        // All channels have a value. Cache it.
        data->ready = true;
    }
    return true;
}

bool rc_data_has_dirty_channels(rc_data_t *data)
{
    if (rc_data_is_ready(data))
    {
        for (unsigned ii = 0; ii < data->channels_num; ii++)
        {
            control_channel_t *ch = &data->channels[ii];
            if (data_state_is_dirty(&ch->data_state))
            {
                return true;
            }
        }
    }
    return false;
}

// Used by the RX to keep track when the channels need to be flushed
// to the FC. ACK seq is not used, since differential updates only
// happen OTA.
void rc_data_channels_sent(rc_data_t *data, time_micros_t now)
{
    for (unsigned ii = 0; ii < data->channels_num; ii++)
    {
        control_channel_t *ch = &data->channels[ii];
        data_state_sent(&ch->data_state, -1, now);
    }
}

unsigned rc_data_get_channel_percentage(const rc_data_t *data, unsigned ch)
{
    return (data->channels[ch].value * 100) / RC_CHANNEL_MAX_VALUE;
}

telemetry_t *rc_data_get_telemetry(rc_data_t *data, int telemetry_id)
{
    if (telemetry_id & TELEMETRY_UPLINK_MASK)
    {
        return &data->telemetry_uplink[TELEMETRY_UPLINK_GET_IDX(telemetry_id)];
    }
    return &data->telemetry_downlink[TELEMETRY_DOWNLINK_GET_IDX(telemetry_id)];
}

telemetry_t *rc_data_get_downlink_telemetry(rc_data_t *data, telemetry_downlink_id_e id)
{
    assert(!(id & TELEMETRY_UPLINK_MASK));
    return &data->telemetry_downlink[id];
}

telemetry_t *rc_data_get_uplink_telemetry(rc_data_t *data, telemetry_uplink_id_e id)
{
    assert(id & TELEMETRY_UPLINK_MASK);
    return &data->telemetry_uplink[TELEMETRY_UPLINK_GET_IDX(id)];
}

const char *rc_data_get_pilot_name(const rc_data_t *data)
{
    const telemetry_t *val = &data->telemetry_uplink[TELEMETRY_UPLINK_GET_IDX(TELEMETRY_ID_PILOT_NAME)];
    return telemetry_has_value(val) && val->val.s[0] ? val->val.s : NULL;
}

const char *rc_data_get_craft_name(const rc_data_t *data)
{
    const telemetry_t *val = &data->telemetry_downlink[TELEMETRY_ID_CRAFT_NAME];
    return telemetry_has_value(val) && val->val.s[0] ? val->val.s : NULL;
}

bool rc_data_input_failsafe_is_active(const rc_data_t *data)
{
    return failsafe_is_active(data->failsafe.input);
}

bool rc_data_output_failsafe_is_active(const rc_data_t *data)
{
    return failsafe_is_active(data->failsafe.output);
}
