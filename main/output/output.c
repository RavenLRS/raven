#include <assert.h>
#include <string.h>

#include <hal/log.h>

#include "config/settings.h"

#include "msp/msp.h"

#include "rc/telemetry.h"

#include "util/macros.h"

#include "output.h"

static const char *TAG = "Output";

#define OUTPUT_FC_MSP_UPDATE_INTERVAL SECS_TO_MICROS(10)
#define MSP_SEND_REQ(output, req) OUTPUT_MSP_SEND_REQ(output, req, output_msp_callback)
#define FW_VARIANT_CONST(s0, s1, s2, s3) (s0 << 24 | s1 << 16 | s2 << 8 | s3)
#define FW_VARIANT_CONST_S(s) FW_VARIANT_CONST(s[0], s[1], s[2], s[3])

static void telemetry_updated_callback(void *data, telemetry_downlink_id_e id, telemetry_val_t *val)
{
    output_t *output = data;
    telemetry_t *telemetry = &output->rc_data->telemetry_downlink[TELEMETRY_DOWNLINK_GET_IDX(id)];
    // TODO: Pass now to the callback
    time_micros_t now = time_micros_now();
    bool changed = false;
    if (!telemetry_value_is_equal(telemetry, id, val))
    {
        changed = true;
        // TODO: Optimize this copy, we don't need to copy the whole value
        // for small types
        telemetry->val = *val;
    }
    data_state_update(&telemetry->data_state, changed, now);
}

static void telemetry_calculate_callback(void *data, telemetry_downlink_id_e id)
{
    output_t *output = data;
    switch (id)
    {
    case TELEMETRY_ID_BAT_REMAINING_P:
    {
        TELEMETRY_ASSERT_TYPE(TELEMETRY_ID_BAT_CAPACITY, TELEMETRY_TYPE_UINT16);
        TELEMETRY_ASSERT_TYPE(TELEMETRY_ID_CURRENT_DRAWN, TELEMETRY_TYPE_INT32);
        telemetry_t *capacity = rc_data_get_telemetry(output->rc_data, TELEMETRY_ID_BAT_CAPACITY);
        telemetry_t *drawn = rc_data_get_telemetry(output->rc_data, TELEMETRY_ID_CURRENT_DRAWN);
        if (capacity->val.u16 > 0 && drawn->val.i32 >= 0)
        {
            int32_t rem = capacity->val.u16 - drawn->val.i32;
            if (rem < 0)
            {
                rem = 0;
            }
            OUTPUT_TELEMETRY_UPDATE_U8(output, TELEMETRY_ID_BAT_REMAINING_P, (100 * rem) / capacity->val.u16);
        }
        break;
    }
    default:
        break;
    }
}

static void output_setting_changed(const setting_t *setting, void *user_data)
{
    output_t *output = user_data;
    if (SETTING_IS(setting, SETTING_KEY_RX_AUTO_CRAFT_NAME))
    {
        // Re-schedule polls to take into account if the setting
        // is enabled or disabled.
        output->fc.next_fw_update = 0;
    }
}

static void output_msp_configure_poll(output_t *output, uint16_t cmd, time_micros_t interval)
{
    for (int ii = 0; ii < OUTPUT_FC_MAX_NUM_POLLS; ii++)
    {
        if (output->fc.polls[ii].interval == 0)
        {
            // We found a free one
            output->fc.polls[ii].cmd = cmd;
            output->fc.polls[ii].interval = interval;
            output->fc.polls[ii].next_poll = 0;
            return;
        }
    }
    // Couldn't find a free slot, we need to raise OUTPUT_FC_MAX_NUM_POLLS
    assert(0 && "Couldn't add MSP poll, raise OUTPUT_FC_MAX_NUM_POLLS");
}

static void output_msp_configure_polling_common(output_t *output)
{
    if (settings_get_key_bool(SETTING_KEY_RX_AUTO_CRAFT_NAME))
    {
        output->craft_name_setting = settings_get_key(SETTING_KEY_RX_CRAFT_NAME);
        output_msp_configure_poll(output, MSP_NAME, SECS_TO_MICROS(10));
    }
    else
    {
        output->craft_name_setting = NULL;
    }
    if (!OUTPUT_HAS_FLAG(output, OUTPUT_FLAG_SENDS_RSSI) && output->fc.rssi.channel_auto)
    {
        output_msp_configure_poll(output, MSP_RSSI_CONFIG, SECS_TO_MICROS(10));
    }
}

static void output_msp_configure_polling_inav(output_t *output)
{
}

static void output_msp_configure_polling_betaflight(output_t *output)
{
}

static void output_msp_configure_polling(output_t *output)
{
    memset(output->fc.polls, 0, sizeof(output->fc.polls));
    output_msp_configure_polling_common(output);
    switch (FW_VARIANT_CONST_S(output->fc.fw_variant))
    {
    case FW_VARIANT_CONST('I', 'N', 'A', 'V'):
        output_msp_configure_polling_inav(output);
        break;
    case FW_VARIANT_CONST('B', 'T', 'F', 'L'):
        output_msp_configure_polling_betaflight(output);
        break;
    default:
        LOG_W(TAG, "Unknown fw_variant \"%.4s\"", output->fc.fw_variant);
    }
}

static void output_msp_callback(msp_conn_t *conn, uint16_t cmd, const void *payload, int size, void *callback_data)
{
    output_t *output = callback_data;
    switch (cmd)
    {
    case MSP_FC_VARIANT:
        if (size == 4)
        {
            memcpy(output->fc.fw_variant, payload, size);
            output->fc.fw_version_is_pending = true;
        }
        break;
    case MSP_FC_VERSION:
        if (size == 3)
        {
            memcpy(output->fc.fw_version, payload, size);
            output_msp_configure_polling(output);
        }
        break;
    case MSP_NAME:
        OUTPUT_TELEMETRY_UPDATE_STRING(output, TELEMETRY_ID_CRAFT_NAME, payload, size);
        if (output->craft_name_setting && size > 0)
        {
            // The string is not null terminated, so we need to ammend it
            char sval[SETTING_STRING_BUFFER_SIZE];
            size_t val_size = MIN(sizeof(sval) - 1, size);
            memcpy(sval, payload, val_size);
            sval[val_size] = '\0';
            setting_set_string(output->craft_name_setting, sval);
        }
        break;
    case MSP_RSSI_CONFIG:
        if (size == 1)
        {
            // MSP based FCs return 0 to mean disabled, non-zero to indicate
            // that the channel number (from 1) is the RSSI channel.
            uint8_t fc_rssi_channel = *((const uint8_t *)payload);
            output->fc.rssi.channel = fc_rssi_channel > 0 ? fc_rssi_channel - 1 : -1;
        }
        break;
    }
}

static void output_msp_poll(output_t *output, time_micros_t now)
{
    if (output->fc.next_fw_update < now)
    {
        if (MSP_SEND_REQ(output, MSP_FC_VARIANT) > 0)
        {
            output->fc.next_fw_update = now + OUTPUT_FC_MSP_UPDATE_INTERVAL;
        }
    }
    else if (output->fc.fw_version_is_pending)
    {
        if (MSP_SEND_REQ(output, MSP_FC_VERSION) > 0)
        {
            output->fc.fw_version_is_pending = false;
        }
    }

    for (int ii = 0; ii < OUTPUT_FC_MAX_NUM_POLLS; ii++)
    {
        time_micros_t interval = output->fc.polls[ii].interval;
        if (interval == 0)
        {
            // Note we don't break because we might support polls that can be
            // enabled or disabled depending on other polls (e.g. GPS features)
            continue;
        }
        if (output->fc.polls[ii].next_poll < now)
        {
            if (MSP_SEND_REQ(output, output->fc.polls[ii].cmd) > 0)
            {
                output->fc.polls[ii].next_poll = now + interval;
            }
        }
    }
}

static void output_configure_rssi(output_t *output)
{
    // TODO: This code and RSSI handling can be left out when no RX support is compiled in
    rx_rssi_channel_e rssi_channel = RX_RSSI_CHANNEL_NONE;
    if (!OUTPUT_HAS_FLAG(output, OUTPUT_FLAG_REMOTE) && !OUTPUT_HAS_FLAG(output, OUTPUT_FLAG_SENDS_RSSI))
    {
        const setting_t *rx_rssi_channel_setting = settings_get_key(SETTING_KEY_RX_RSSI_CHANNEL);
        if (rx_rssi_channel_setting)
        {
            rssi_channel = setting_get_u8(rx_rssi_channel_setting);
        }
    }
    switch (rssi_channel)
    {
    case RX_RSSI_CHANNEL_AUTO:
        output->fc.rssi.channel_auto = true;
        output->fc.rssi.channel = -1;
        break;
    case RX_RSSI_CHANNEL_NONE:
        output->fc.rssi.channel_auto = false;
        output->fc.rssi.channel = -1;
        break;
    default:
        output->fc.rssi.channel_auto = false;
        output->fc.rssi.channel = rx_rssi_channel_index(rssi_channel);
        break;
    }
}

bool output_open(rc_data_t *data, output_t *output, void *config)
{
    bool is_open = false;
    if (output)
    {
        failsafe_init(&output->failsafe);
        rc_data_reset_output(data);
        output->rc_data = data;
        memset(&output->fc, 0, sizeof(output_fc_t));
        output_configure_rssi(output);
        output->telemetry_updated = telemetry_updated_callback;
        output->telemetry_calculate = telemetry_calculate_callback;
        output->craft_name_setting = NULL;
        if (!output->is_open && output->vtable.open)
        {
            output->min_rc_update_interval = 0;
            // If the data is slower than 5hz, repeat frames
            output->max_rc_update_interval = FREQ_TO_MICROS(5);
            output->next_rc_update_no_earlier_than = 0;
            output->next_rc_update_no_later_than = TIME_MICROS_MAX;
            is_open = output->is_open = output->vtable.open(output, config);
            if (is_open)
            {
                settings_add_listener(output_setting_changed, output);
            }
        }
    }
    return is_open;
}

bool output_update(output_t *output, bool input_was_updated, time_micros_t now)
{
    bool updated = false;
    if (output && output->is_open)
    {
        // Update RC control
        bool update_rc = false;
        uint16_t channel_value;
        control_channel_t *rssi_channel = NULL;
        bool can_update_already = now > output->next_rc_update_no_earlier_than;
        bool can_update_via_max_interval = now > output->next_rc_update_no_later_than &&
                                           !failsafe_is_active(output->rc_data->failsafe.input);
        bool can_update_via_new_data = input_was_updated || rc_data_has_dirty_channels(output->rc_data);
        if (can_update_already && (can_update_via_max_interval || can_update_via_new_data))
        {
            update_rc = true;

            if (output->fc.rssi.channel >= 0 && output->fc.rssi.channel < RC_CHANNELS_NUM)
            {
                rssi_channel = &output->rc_data->channels[output->fc.rssi.channel];
                uint8_t lq = CONSTRAIN(TELEMETRY_GET_I8(output->rc_data, TELEMETRY_ID_RX_LINK_QUALITY), 0, 100);
                channel_value = rssi_channel->value;
                rssi_channel->value = RC_CHANNEL_VALUE_FROM_PERCENTAGE(lq);
            }
        }
        updated = output->vtable.update(output, output->rc_data, update_rc, now);
        if (updated && update_rc)
        {
            rc_data_channels_sent(output->rc_data, now);
            output->next_rc_update_no_earlier_than = now + output->min_rc_update_interval;
            if (output->max_rc_update_interval > 0)
            {
                output->next_rc_update_no_later_than = now + output->max_rc_update_interval;
            }
            else
            {
                output->next_rc_update_no_later_than = TIME_MICROS_MAX;
            }
        }
        // Restore the channel data we ovewrote
        if (rssi_channel)
        {
            rssi_channel->value = channel_value;
        }
        failsafe_update(&output->failsafe, time_micros_now());

        // Read MSP transport responses (if any)
        if (msp_io_is_connected(&output->msp))
        {
            // make sure we don't double poll via MSP by polling
            // in the air outputs.
            if (!OUTPUT_HAS_FLAG(output, OUTPUT_FLAG_REMOTE))
            {
                output_msp_poll(output, now);
            }
            msp_io_update(&output->msp);
        }
    }
    return updated;
}

void output_close(output_t *output, void *config)
{
    if (output && output->is_open && output->vtable.close)
    {
        output->vtable.close(output, config);
        output->is_open = false;
        settings_remove_listener(output_setting_changed, output);
    }
}
