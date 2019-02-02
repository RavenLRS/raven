#include <stdio.h>

#include <hal/log.h>

#include "config/settings_rmp.h"

#include "protocols/crsf_units.h"

#include "rc/rc_data.h"

#include "rmp/rmp.h"

#include "util/version.h"

#include "input_crsf.h"

static const char *TAG = "CRSF.Input";

#define BPS_DETECT_SWITCH_INTERVAL_US MILLIS_TO_MICROS(1000)
#define BPS_FALLBACK_INTERVAL SECS_TO_MICROS(1)
#define RESPONSE_WAIT_INTERVAL_US (500)

#define CRSF_INPUT_SETTINGS_VIEW ((settings_rmp_view_t){.id = SETTINGS_VIEW_CRSF_INPUT, .folder_id = FOLDER_ID_ROOT, .recursive = true})

#define CRSF_INPUT_CMD_PING 48 // Sent by CRSF to "ping" a CMD setting that's being manipulated

// XXX: OpenTX 2.2.1 doesn't consider -1dBm a valid value and ignores it.
// To workaround the problem we send -1 as 0.
// TODO: Check if OpenTX 2.2.2 fixes it.
#define CRSF_RSSI_DBM(d) ({ \
    int8_t __rssi = d;      \
    if (__rssi == -1)       \
    {                       \
        __rssi = 0;         \
    }                       \
    __rssi;                 \
})

// These are the CRSF telemetry frames understood by OpenTX. We send
// a frame every cycle after the radio has finished transmitting.
static const crsf_frame_type_e radio_telemetry_frames[] = {
    CRSF_FRAMETYPE_GPS,
    CRSF_FRAMETYPE_BATTERY_SENSOR,
    CRSF_FRAMETYPE_LINK_STATISTICS,
    CRSF_FRAMETYPE_ATTITUDE,
    CRSF_FRAMETYPE_FLIGHT_MODE,
};

static void input_crsf_isr(const serial_port_t *port, uint8_t b, void *user_data)
{
    input_crsf_t *input = user_data;
    // OpenTX will always send the frames starting with CRSF_ADDRESS_CRSF_TRANSMITTER
    // so we can use that to synchronize frames in case of some input error.
    if (crsf_port_has_buffered_data(&input->crsf) || b == CRSF_ADDRESS_CRSF_TRANSMITTER)
    {
        // Received byte in RX mode
        time_micros_t now = time_micros_now();
        input->last_byte_at = now;
        crsf_port_push(&input->crsf, b);
    }
}

static unsigned input_crsf_tx_done_timeout_us(input_crsf_t *input)
{
    switch (input->baud_rate)
    {
    case CRSF_OPENTX_BAUDRATE:
        return 25;
    case CRSF_OPENTX_SLOW_BAUDRATE:
        return 100;
    }
    UNREACHABLE();
    return 0;
}

static unsigned input_crsf_frame_interval_us(input_crsf_t *input)
{
    switch (input->bps)
    {
    case CRSF_INPUT_BPS_DETECT:
        return 0;
    case CRSF_INPUT_BPS_400K:
        return 4000;
    case CRSF_INPUT_BPS_115K:
        return 16000;
    }
    return 0;
}

static unsigned input_crsf_max_tx_time_us(input_crsf_t *input)
{
    switch (input->baud_rate)
    {
    case CRSF_OPENTX_BAUDRATE:
        return 2500;
    case CRSF_OPENTX_SLOW_BAUDRATE:
        return 10000;
    }
    return 0;
}

static void input_crsf_get_rmp_addr(input_crsf_t *input_crsf, air_addr_t *addr, uint8_t crsf_addr)
{
    if (crsf_addr != CRSF_ADDRESS_BROADCAST && crsf_addr - 1 < ARRAY_COUNT(input_crsf->rmp_addresses) &&
        air_addr_is_valid(&input_crsf->rmp_addresses[crsf_addr - 1].addr))
    {

        air_addr_cpy(addr, &input_crsf->rmp_addresses[crsf_addr - 1].addr);
    }
    else
    {
        // Broadcast or unknown address. We broadcast the message either way
        *addr = *AIR_ADDR_BROADCAST;
    }
}

static void input_crsf_request_setting(input_crsf_t *input_crsf, uint8_t crsf_addr, uint8_t param_index)
{
    settings_rmp_msg_t req = {
        .code = SETTINGS_RMP_READ_REQ,
    };

    req.read_req.view = CRSF_INPUT_SETTINGS_VIEW;
    // CRSF settings are 1-indexed
    req.read_req.setting_index = param_index - 1;
    air_addr_t dst;
    input_crsf_get_rmp_addr(input_crsf, &dst, crsf_addr);
    rmp_send_flags(input_crsf->input.rc_data->rmp, input_crsf->rmp_port,
                   &dst, RMP_PORT_SETTINGS,
                   &req, settings_rmp_msg_size(&req),
                   RMP_SEND_FLAG_BROADCAST_SELF | RMP_SEND_FLAG_BROADCAST_RC);
}

static void input_crsf_frame_callback(void *data, crsf_frame_t *frame)
{
    input_crsf_t *input_crsf = data;
    time_micros_t now = time_micros_now();
    switch (frame->header.type)
    {
    case CRSF_FRAMETYPE_RC_CHANNELS_PACKED:
    {
        crsf_channels_t *channels = &frame->channels;
        rc_data_update_channel(input_crsf->input.rc_data, 0, channels->ch0, now);
        rc_data_update_channel(input_crsf->input.rc_data, 1, channels->ch1, now);
        rc_data_update_channel(input_crsf->input.rc_data, 2, channels->ch2, now);
        rc_data_update_channel(input_crsf->input.rc_data, 3, channels->ch3, now);
        rc_data_update_channel(input_crsf->input.rc_data, 4, channels->ch4, now);
        rc_data_update_channel(input_crsf->input.rc_data, 5, channels->ch5, now);
        rc_data_update_channel(input_crsf->input.rc_data, 6, channels->ch6, now);
        rc_data_update_channel(input_crsf->input.rc_data, 7, channels->ch7, now);
        rc_data_update_channel(input_crsf->input.rc_data, 8, channels->ch8, now);
        rc_data_update_channel(input_crsf->input.rc_data, 9, channels->ch9, now);
        rc_data_update_channel(input_crsf->input.rc_data, 10, channels->ch10, now);
        rc_data_update_channel(input_crsf->input.rc_data, 11, channels->ch11, now);
        rc_data_update_channel(input_crsf->input.rc_data, 12, channels->ch12, now);
        rc_data_update_channel(input_crsf->input.rc_data, 13, channels->ch13, now);
        rc_data_update_channel(input_crsf->input.rc_data, 14, channels->ch14, now);
        rc_data_update_channel(input_crsf->input.rc_data, 15, channels->ch15, now);
        break;
    }
    case CRSF_FRAMETYPE_MSP_REQ:
        // CRSF_FRAMETYPE_MSP_REQ and CRSF_FRAMETYPE_MSP_WRITE are both for MSP requests,
        // the difference is that CRSF_FRAMETYPE_MSP_REQ is for read request and
        // CRSF_FRAMETYPE_MSP_WRITE for write requests.
    case CRSF_FRAMETYPE_MSP_WRITE:
    {
        crsf_ext_frame_t *msp_frame = crsf_frame_to_ext(frame);
        msp_telemetry_push_request_chunk(&input_crsf->msp_telemetry, msp_frame->msp.payload, crsf_ext_frame_payload_size(msp_frame));
        break;
    }
    case CRSF_FRAMETYPE_DEVICE_PING:
    {
        settings_rmp_msg_t req = {
            .code = SETTINGS_RMP_HELO,
            .helo.view = CRSF_INPUT_SETTINGS_VIEW,
        };
        air_addr_t dst;
        crsf_ext_frame_t *ping_frame = crsf_frame_to_ext(frame);
        input_crsf_get_rmp_addr(input_crsf, &dst, ping_frame->header.dest_addr);
        input_crsf->ping_filter = ping_frame->header.dest_addr;
        if (ping_frame->header.dest_addr != CRSF_ADDRESS_BROADCAST)
        {
            // Requesting a device specific ping. Empty the scheduled frames,
            // since we might have responses for other devices.
            ring_buffer_discard(&input_crsf->scheduled);
        }
        rmp_send_flags(input_crsf->input.rc_data->rmp, input_crsf->rmp_port,
                       &dst, RMP_PORT_SETTINGS,
                       &req, settings_rmp_msg_size(&req),
                       RMP_SEND_FLAG_BROADCAST_SELF | RMP_SEND_FLAG_BROADCAST_RC);

        break;
    }
    case CRSF_FRAMETYPE_PARAMETER_READ:
    {
        crsf_ext_frame_t *read_frame = crsf_frame_to_ext(frame);
        input_crsf_request_setting(input_crsf, read_frame->header.dest_addr, read_frame->parameter_read.param_index);
        break;
    }
    case CRSF_FRAMETYPE_PARAMETER_WRITE:
    {
        // Since we need the setting to decide which parts of the CRSF
        // payload we want to read, we request the setting again.
        crsf_ext_frame_t *write_frame = crsf_frame_to_ext(frame);
        uint8_t dest_addr = write_frame->header.dest_addr;
        if (dest_addr < ARRAY_COUNT(input_crsf->rmp_addresses))
        {
            memcpy(&input_crsf->rmp_addresses[dest_addr].pending_write,
                   &write_frame->parameter_write, sizeof(write_frame->parameter_write));
            input_crsf_request_setting(input_crsf, dest_addr, write_frame->parameter_write.param_index);
        }
        break;
    }
    default:
        LOG_W(TAG, "Unknown frame type 0x%02x with size %u", frame->header.type, frame->header.frame_size);
        LOG_BUFFER_W(TAG, frame, frame->header.frame_size + CRSF_FRAME_NOT_COUNTED_BYTES);
    }

    // Note that we always should get a frame with the same period. When there's extra data from
    // the radio (e.g. an MSP request), it will replace control frames.
    unsigned frame_interval = input_crsf_frame_interval_us(input_crsf);
    if (frame_interval > 0)
    {
        if (input_crsf->last_frame_recv > 0 && now - input_crsf->last_frame_recv > frame_interval * 1.5f)
        {
            LOG_W(TAG, "Lost a frame, interval between 2 was %llu", now - input_crsf->last_frame_recv);
        }
    }
    input_crsf->last_frame_recv = now;
}

static int input_crsf_write(void *arg, const void *buf, size_t size)
{
    // UART_FIFO_LEN is 128, while CRSF_FRAME_SIZE_MAX is 64. Since we're
    // sending one frame at a time every cycle, we should
    // be fine not handling the case when the buffer is full.
    input_crsf_t *input_crsf = arg;
    return serial_port_write(input_crsf->serial_port, buf, size);
}

static crsf_rf_power_e input_crsf_get_tx_power(input_crsf_t *input)
{
    int8_t dbm = TELEMETRY_GET_I8(input->input.rc_data, TELEMETRY_ID_TX_RF_POWER);
    if (dbm >= 33)
    {
        return CRSF_RF_POWER_2000_mW;
    }
    if (dbm >= 30)
    {
        return CRSF_RF_POWER_1000_mW;
    }
    if (dbm >= 27)
    {
        return CRSF_RF_POWER_500_mW;
    }
    if (dbm >= 20)
    {
        return CRSF_RF_POWER_100_mW;
    }
    if (dbm >= 14)
    {
        return CRSF_RF_POWER_25_mW;
    }
    if (dbm >= 10)
    {
        return CRSF_RF_POWER_10_mW;
    }
    return CRSF_RF_POWER_0_mW;
}

static void input_crsf_send_telemetry_frame(input_crsf_t *input, crsf_frame_type_e frame_type)
{
    rc_data_t *rc_data = input->input.rc_data;

    crsf_frame_t frame = {
        .header = {
            // OpenTX will only accept frames with this addr
            .device_addr = CRSF_ADDRESS_RADIO_TRANSMITTER,
            .type = frame_type,
        },
    };
    //LOG_I(TAG, "Will send frame type 0x%02x at index %d", frame_type, input->telemetry_pos);
    switch (frame_type)
    {
    case CRSF_FRAMETYPE_GPS:
        frame.header.frame_size = CRSF_FRAME_SIZE(sizeof(crsf_gps_t));
        frame.gps.lat = coord_to_crsf_coord(TELEMETRY_GET_I32(rc_data, TELEMETRY_ID_GPS_LAT));
        frame.gps.lon = coord_to_crsf_coord(TELEMETRY_GET_I32(rc_data, TELEMETRY_ID_GPS_LON));
        frame.gps.ground_speed = speed_to_crsf_speed(TELEMETRY_GET_U16(rc_data, TELEMETRY_ID_GPS_SPEED));
        frame.gps.heading = heading_to_crsf_heading(TELEMETRY_GET_U16(rc_data, TELEMETRY_ID_GPS_HEADING));
        frame.gps.altitude = alt_to_crsf_alt(TELEMETRY_GET_I32(rc_data, TELEMETRY_ID_GPS_ALT));
        frame.gps.sats = TELEMETRY_GET_U8(rc_data, TELEMETRY_ID_GPS_NUM_SATS);
        break;
    case CRSF_FRAMETYPE_BATTERY_SENSOR:
        frame.header.frame_size = CRSF_FRAME_SIZE(sizeof(crsf_battery_sensor_t));
        frame.battery_sensor.voltage = volts_to_crsf_volts(TELEMETRY_GET_U16(rc_data, TELEMETRY_ID_BAT_VOLTAGE));
        frame.battery_sensor.current = amps_to_crsf_amps(TELEMETRY_GET_I16(rc_data, TELEMETRY_ID_CURRENT));
        frame.battery_sensor.mah_drawn = mah_to_crsf_mah(TELEMETRY_GET_I32(rc_data, TELEMETRY_ID_CURRENT_DRAWN));
        frame.battery_sensor.percentage_remaining = TELEMETRY_GET_U8(rc_data, TELEMETRY_ID_BAT_REMAINING_P);
        break;
    case CRSF_FRAMETYPE_LINK_STATISTICS:
        if (failsafe_is_active(rc_data->failsafe.output))
        {
            // Don't send this frame if the output is in FS
            // (RC link is broken), so the radio will play
            // the "telemetry lost sound"
            return;
        }
        frame.header.frame_size = CRSF_FRAME_SIZE(sizeof(crsf_link_stats_t));
        frame.stats.uplink_rssi_ant1 = CRSF_RSSI_DBM(TELEMETRY_GET_I8(rc_data, TELEMETRY_ID_RX_RSSI_ANT1));
        frame.stats.uplink_rssi_ant2 = CRSF_RSSI_DBM(TELEMETRY_GET_I8(rc_data, TELEMETRY_ID_RX_RSSI_ANT2));
        frame.stats.uplink_lq = TELEMETRY_GET_I8(rc_data, TELEMETRY_ID_RX_LINK_QUALITY);
        frame.stats.uplink_snr = TELEMETRY_GET_I8(rc_data, TELEMETRY_ID_RX_SNR) / TELEMETRY_SNR_MULTIPLIER;
        // TODO: Values > 127 won't be correctly displayed. Should we make it an
        // int8_t telemetry value?
        frame.stats.active_antenna = TELEMETRY_GET_U8(rc_data, TELEMETRY_ID_RX_ACTIVE_ANT);
        frame.stats.rf_mode = 0; // TODO
        frame.stats.uplink_tx_power = input_crsf_get_tx_power(input);
        frame.stats.downlink_rssi = CRSF_RSSI_DBM(TELEMETRY_GET_I8(rc_data, TELEMETRY_ID_TX_RSSI_ANT1));
        frame.stats.downlink_lq = TELEMETRY_GET_I8(rc_data, TELEMETRY_ID_TX_LINK_QUALITY);
        frame.stats.downlink_snr = TELEMETRY_GET_I8(rc_data, TELEMETRY_ID_TX_SNR) / TELEMETRY_SNR_MULTIPLIER;
        break;
    case CRSF_FRAMETYPE_ATTITUDE:
        frame.header.frame_size = CRSF_FRAME_SIZE(sizeof(crsf_attitude_t));
        frame.attitude.pitch = deg_to_crsf_rad(TELEMETRY_GET_I16(rc_data, TELEMETRY_ID_ATTITUDE_X));
        frame.attitude.roll = deg_to_crsf_rad(TELEMETRY_GET_I16(rc_data, TELEMETRY_ID_ATTITUDE_Y));
        frame.attitude.yaw = deg_to_crsf_rad(TELEMETRY_GET_I16(rc_data, TELEMETRY_ID_ATTITUDE_Z));
        break;
    case CRSF_FRAMETYPE_FLIGHT_MODE:
        crsf_frame_put_str(&frame, TELEMETRY_GET_STR(rc_data, TELEMETRY_ID_FLIGHT_MODE_NAME));
        break;
    default:
        // Not a telemetry frame, should't reach this
        LOG_E(TAG, "Unhandled telemetry frame 0x%02x at pos %d", frame_type, input->telemetry_pos);
        ASSERT(0 && "Unhandled telemetry frame!");
    }
    crsf_port_write(&input->crsf, &frame);
}

static bool input_crsf_send_msp(input_crsf_t *input)
{
    crsf_ext_frame_t frame;
    size_t n;
    if ((n = msp_telemetry_pop_response_chunk(&input->msp_telemetry, frame.msp.payload)) > 0)
    {
        //LOG_I(TAG, "Sending MSP response");
        //LOG_BUFFER_I(TAG, frame.msp.payload, n);
        frame.header.device_addr = CRSF_ADDRESS_RADIO_TRANSMITTER;
        frame.header.type = CRSF_FRAMETYPE_MSP_RESP;
        frame.header.frame_size = CRSF_EXT_FRAME_SIZE(CRSF_MSP_RESP_PAYLOAD_SIZE);
        memset(&frame.msp.payload[n], 0, CRSF_MSP_RESP_PAYLOAD_SIZE - n);
        frame.header.dest_addr = CRSF_ADDRESS_RADIO_TRANSMITTER;
        frame.header.orig_addr = CRSF_ADDRESS_FLIGHT_CONTROLLER;
        crsf_port_write(&input->crsf, crsf_ext_frame_to_frame(&frame));
        return true;
    }
    return false;
}

static bool input_crsf_send_scheduled_frame(input_crsf_t *input)
{
    crsf_frame_t frame;
    if (ring_buffer_pop(&input->scheduled, &frame))
    {
        frame.header.device_addr = CRSF_ADDRESS_RADIO_TRANSMITTER;
        crsf_port_write(&input->crsf, &frame);
        return true;
    }
    return false;
}

static bool input_crsf_send_pending_resp_frame(input_crsf_t *input)
{
    return input_crsf_send_scheduled_frame(input) || input_crsf_send_msp(input);
}

static void input_crsf_send_response(input_crsf_t *input)
{
    crsf_frame_type_e frame_type = radio_telemetry_frames[input->telemetry_pos];
    serial_port_begin_write(input->serial_port);
    // If we're not sending the link stats, check if we have a pending response data to send
    if (frame_type == CRSF_FRAMETYPE_LINK_STATISTICS || !input_crsf_send_pending_resp_frame(input))
    {
        // Write one telemetry frame
        input_crsf_send_telemetry_frame(input, frame_type);
    }
    if (++input->telemetry_pos >= ARRAY_COUNT(radio_telemetry_frames))
    {
        input->telemetry_pos = 0;
    }
    serial_port_end_write(input->serial_port);
}

static void input_crsf_update_baud_rate(input_crsf_t *input, unsigned baud_rate)
{
    // Stored here because uart_get_baudrate() might return slightly
    // different values due to the frequency being divisible by the
    // xtal frequency
    input->baud_rate = baud_rate;
    LOG_D(TAG, "Switching to baud rate %u", baud_rate);
    serial_port_set_baudrate(input->serial_port, baud_rate);
}

static void input_crsf_send_setting_frame(input_crsf_t *input_crsf, const settings_rmp_setting_t *setting, uint8_t crsf_src_addr)
{
#define CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY_RB_SIZE (CRSF_MAX_SETTINGS_ENTRY_PAYLOAD_SIZE * 4)
#define RB_PUSH_STR(rb, str)                 \
    do                                       \
    {                                        \
        const char *c;                       \
        for (c = str; *c; c++)               \
        {                                    \
            ASSERT(ring_buffer_push(rb, c)); \
        }                                    \
        ASSERT(ring_buffer_push(rb, c));     \
    } while (0)
    uint8_t bb;
    // A parameter settings entry might need several packets, so we use
    // a ring buffer and then split it with chunking as needed.
    RING_BUFFER_DECLARE_VAR(rb, b, uint8_t, CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY_RB_SIZE);
    RING_BUFFER_INIT(&rb.b, uint8_t, CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY_RB_SIZE);
    crsf_ext_frame_t entry = {
        .header.device_addr = CRSF_ADDRESS_RADIO_TRANSMITTER,
        .header.type = CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,
        .header.orig_addr = crsf_src_addr,
        .header.dest_addr = CRSF_ADDRESS_RADIO_TRANSMITTER,
    };
    // CRSF settings are 1-indexed
    entry.settings_entry.param_index = setting->setting_index + 1;
    // Parent index is -1 when there's no parent, so it maps nicely to zero
    // which means "no parent" in CRSF settings
    entry.settings_entry.parent_folder_param_index = setting->parent_index + 1;
    // Now start filling the buffer. First the name
    RB_PUSH_STR(&rb.b, settings_rmp_setting_get_name(setting));
    if (setting->flags & SETTING_FLAG_NAME_MAP)
    {
        entry.settings_entry.type = CRSF_TEXT_SELECTION;
        // Text selection sends possible the values ; separated followed
        // by a zero. Then the current value, followed by the min, max
        // and default values (all uint8_t), the followed by a string
        // indicating the unit (or suffix).
        unsigned name_count = settings_rmp_setting_get_mapped_names_count(setting);
        char val_name_buf[16];
        for (unsigned ii = 0; ii < name_count; ii++)
        {
            const char *val_name = settings_rmp_setting_get_mapped_name(setting, ii);
            if (!val_name)
            {
                // Make sure malformed packets make us don't crash
                snprintf(val_name_buf, sizeof(val_name_buf), "%u", ii);
                val_name = val_name_buf;
            }
            const char *c;
            for (c = val_name; *c; c++)
            {
                ASSERT(ring_buffer_push(&rb.b, c));
            }
            if (ii == name_count - 1)
            {
                // Push a null
                ASSERT(ring_buffer_push(&rb.b, c));
            }
            else
            {
                // Push the separator
                uint8_t sep = ';';
                ASSERT(ring_buffer_push(&rb.b, &sep));
            }
        }
        // Actual value
        bb = settings_rmp_setting_get_value(setting);
        ASSERT(ring_buffer_push(&rb.b, &bb));
        // Min
        bb = settings_rmp_setting_get_min(setting);
        ASSERT(ring_buffer_push(&rb.b, &bb));
        // Max
        bb = settings_rmp_setting_get_max(setting);
        ASSERT(ring_buffer_push(&rb.b, &bb));
        // Default
        bb = settings_rmp_setting_get_min(setting);
        ASSERT(ring_buffer_push(&rb.b, &bb));
    }
    else if (setting->flags & SETTING_FLAG_CMD)
    {
        entry.settings_entry.type = CRSF_COMMAND;
        // Command field has the following structure
        // status: uint8_t (value)
        // timeout: uint8_t (0.01s until the radio queries the state again)
        // info: null terminated string which is dislayed
        // like [info] at the right of the command name
        //
        // The CRSF_COMMAND works like:
        // 1) Setting is sent as 0 to the radio. Otherwise clicking does nothing.
        // 2) When the user toggles the setting, radio writes 1 and waits
        // 3a) If device replies with 2, radio shows a warning
        // 3b) If device replies with 3, radio asks for confirmation
        // 4) While the popup is running, radio writes 48 (seems related to the
        //  total number of settings - see fieldChunk in device.lua) to the
        //  setting every timeout.
        // 5) OK writes a 4 (only possible with 3b), cancel writes a 5
        // 6) Radio keeps the popup displayed until it gets a response with value < 2 || > 3
        //
        // This dance is mostly implemented by settings.c, so we just need to do a bit
        // of processing here for the info string.
        bb = settings_rmp_setting_get_value(setting);
        ASSERT(ring_buffer_push(&rb.b, &bb));
        bb = 0xFF;
        ASSERT(ring_buffer_push(&rb.b, &bb));
        setting_cmd_flag_e cmd_flags = settings_rmp_setting_cmd_get_flags(setting);
        bool info_written = false;
        if (cmd_flags & SETTING_CMD_FLAG_CONFIRM)
        {
            if (settings_rmp_setting_get_value(setting) > 0)
            {
                RB_PUSH_STR(&rb.b, "Confirm");
                info_written = true;
            }
            else
            {
                RB_PUSH_STR(&rb.b, "*");
                info_written = true;
            }
        }
        if (!info_written)
        {
            RB_PUSH_STR(&rb.b, "");
        }
    }
    else
    {
        switch (setting->type)
        {
        case SETTING_TYPE_U8:
        {
            entry.settings_entry.type = CRSF_UINT8;
            uint8_t u8b;
            // Actual value
            u8b = settings_rmp_setting_get_value(setting);
            ASSERT(ring_buffer_push(&rb.b, &u8b));
            // Min
            u8b = settings_rmp_setting_get_min(setting);
            ASSERT(ring_buffer_push(&rb.b, &u8b));
            // Max
            u8b = settings_rmp_setting_get_max(setting);
            ASSERT(ring_buffer_push(&rb.b, &u8b));
            // Default
            u8b = settings_rmp_setting_get_min(setting);
            ASSERT(ring_buffer_push(&rb.b, &u8b));
            break;
        }
        case SETTING_TYPE_FOLDER:
            entry.settings_entry.type = CRSF_FOLDER;
            break;
        case SETTING_TYPE_STRING:
        {
            const char *value = settings_rmp_setting_get_str_value(setting);
            if (setting->flags & SETTING_FLAG_READONLY)
            {
                // Just the string value for display
                entry.settings_entry.type = CRSF_INFO;
                RB_PUSH_STR(&rb.b, value);
            }
            else
            {
                // String value followed by optional max length as an uint8_t
                entry.settings_entry.type = CRSF_STRING;
                // If we have an empty string, send a blank char instead so the
                // UI can show the selection. It works anyway, but it looks better
                // when there's something show the currently selected field.
                if (!value || strlen(value) == 0)
                {
                    value = " ";
                }
                RB_PUSH_STR(&rb.b, value);
                uint8_t max_size = settings_rmp_setting_get_str_max_length(setting);
                ASSERT(ring_buffer_push(&rb.b, &max_size));
            }
            break;
        }
        default:
            LOG_W(TAG, "Unhandled setting '%s' type %d for CRSF input", settings_rmp_setting_get_name(setting), setting->type);
            UNREACHABLE();
        }
    }
    // TODO: Units, supported by all setting types. For now we just put
    // a null byte (no unit)
    if (0 /*setting->unit*/)
    {
        //RB_PUSH_STR(&rb.b, setting->unit);
    }
    else
    {
        uint8_t zero = '\0';
        ASSERT(ring_buffer_push(&rb.b, &zero));
    }
    // XXX: Due to a bug in device.lua, if we send CRSF_MAX_SETTINGS_ENTRY_PAYLOAD_SIZE,
    // some chunks will be incorrectly interpreted and the current value will be
    // incorrectly displayed (e.g. value = 0 shows value = 2 for the RSSI channel setting).
    const int max_setting_payload_size = CRSF_MAX_SETTINGS_ENTRY_PAYLOAD_SIZE - 8;
    int cur_chunk = ring_buffer_count(&rb.b) / max_setting_payload_size;
    while (cur_chunk >= 0)
    {
        size_t chunk_payload_size = MIN(ring_buffer_count(&rb.b), max_setting_payload_size);
        entry.settings_entry.chunk = cur_chunk;
        entry.header.frame_size = CRSF_SETTINGS_ENTRY_FRAME_SIZE(chunk_payload_size);
        uint8_t *ptr = entry.settings_entry.payload;
        for (int ii = 0; ii < chunk_payload_size; ii++)
        {
            ASSERT(ring_buffer_pop(&rb.b, ptr++));
        }
        ring_buffer_push(&input_crsf->scheduled, &entry);
        cur_chunk--;
    }
    ASSERT(ring_buffer_count(&rb.b) == 0);
}

static void input_crsf_rmp_handler(rmp_t *rmp, rmp_req_t *req, void *user_data)
{
    input_crsf_t *input = user_data;
    if (!settings_rmp_msg_is_valid(req->msg))
    {
        return;
    }

    // Check if we already know the device addr
    int device_addr = -1;
    for (int ii = 0; ii < ARRAY_COUNT(input->rmp_addresses); ii++)
    {
        if (air_addr_equals(&req->msg->src, &input->rmp_addresses[ii].addr))
        {
            device_addr = ii + 1;
            input->rmp_addresses[ii].last_seen = time_micros_now();
            break;
        }
    }

    if (device_addr < 0)
    {
        // Not known device addr. Try to register.
        for (int ii = 0; ii < ARRAY_COUNT(input->rmp_addresses); ii++)
        {
            if (!air_addr_is_valid(&input->rmp_addresses[ii].addr) ||
                time_micros_now() - input->rmp_addresses[ii].last_seen > 10e6)
            {
                // Slot is free or device hasn't been seen for 10s
                device_addr = ii + 1;
                air_addr_cpy(&input->rmp_addresses[ii].addr, &req->msg->src);
                input->rmp_addresses[ii].last_seen = time_micros_now();
                break;
            }
        }

        if (device_addr < 0)
        {
            // Could not register device
            return;
        }
    }

    const settings_rmp_msg_t *msg = req->msg->payload;
    switch ((settings_rmp_code_e)msg->code)
    {
    case SETTINGS_RMP_EHLO:
    {
        if (input->ping_filter != CRSF_ADDRESS_BROADCAST && input->ping_filter != device_addr)
        {
            // We're waiting for a ping from another device
            break;
        }
        crsf_frame_t frame;
        crsf_ext_frame_t *info = crsf_frame_to_ext(&frame);
        info->header.orig_addr = device_addr;
        info->header.type = CRSF_FRAMETYPE_DEVICE_INFO;
        info->header.dest_addr = CRSF_ADDRESS_RADIO_TRANSMITTER;
        strlcpy(info->device_info.name, msg->ehlo.name, sizeof(info->device_info.name));
        crsf_device_info_tail_t *tail = crsf_device_info_get_tail(&info->device_info);
        tail->null1 = 0;
        tail->null2 = 0;
        tail->null3 = 0;
        tail->parameter_count = msg->ehlo.settings_count;
        tail->info_version = 1;
        info->header.frame_size = CRSF_EXT_FRAME_SIZE(strlen(info->device_info.name) + 1 + sizeof(crsf_device_info_tail_t));
        ring_buffer_push(&input->scheduled, &frame);
        break;
    }
    case SETTINGS_RMP_READ:
    {
        const settings_rmp_setting_t *setting = &msg->setting;
        // Check if we have a pending write
        crsf_parameter_write_t *pending_write = &input->rmp_addresses[device_addr].pending_write;
        if (pending_write->param_index > 0 && pending_write->param_index - 1 == setting->setting_index)
        {
            // CRSF scripts will happily send us writes for non-editable
            // settings (e.g. CRSF_INFO)
            if (!(setting->flags & SETTING_FLAG_READONLY))
            {
                settings_rmp_setting_t cpy;
                memcpy(&cpy, setting, sizeof(cpy));
                bool do_write = true;
                switch (setting->type)
                {
                case SETTING_TYPE_U8:
                    if (setting->flags & SETTING_FLAG_CMD)
                    {
                        if (pending_write->payload.u8 == CRSF_INPUT_CMD_PING)
                        {
                            // Convert to the value expected by settings.c
                            pending_write->payload.u8 = SETTING_CMD_STATUS_PING;
                        }
                    }
                    do_write = settings_rmp_setting_set_value(&cpy, pending_write->payload.u8);
                    break;
                case SETTING_TYPE_STRING:
                    // Since the crossfire.lua script doesn't support deleting characters, the
                    // only way to shorten a string is to set the characters at the end as blank
                    // so we need to trim them manually.

                    // Make sure there's at least a null at the end before we call strlen()
                    pending_write->payload.s[sizeof(pending_write->payload.s) - 1] = '\0';
                    for (int ii = (int)strlen(pending_write->payload.s) - 1; ii >= 0; ii--)
                    {
                        if (pending_write->payload.s[ii] != ' ')
                        {
                            // Non blank, stop
                            break;
                        }
                        pending_write->payload.s[ii] = '\0';
                    }
                    do_write = settings_rmp_setting_set_str_value(&cpy, pending_write->payload.s);
                    break;

                default:
                    do_write = false;
                    LOG_W(TAG, "Can't handle CRSF write for setting of type %d", setting->type);
                }
                if (do_write)
                {
                    settings_rmp_msg_t write_req;
                    settings_rmp_setting_prepare_write(&cpy, &write_req);
                    rmp_send_flags(rmp, input->rmp_port, &req->msg->src, RMP_PORT_SETTINGS,
                                   &write_req, settings_rmp_msg_size(&write_req),
                                   RMP_SEND_FLAG_BROADCAST_SELF | RMP_SEND_FLAG_BROADCAST_RC);
                }
            }
            memset(pending_write, 0, sizeof(*pending_write));
        }
        else
        {
            input_crsf_send_setting_frame(input, setting, device_addr);
        }
        break;
    }
    case SETTINGS_RMP_WRITE:
    {
        // Send the setting with the new value. This causes the CRSF
        // OpenTX script to reload the folder, so settings that have
        // changed visibility get updated.
        const settings_rmp_setting_t *setting = &msg->setting;
        input_crsf_send_setting_frame(input, setting, device_addr);
        break;
    }
    // Requests, not handled here:
    case SETTINGS_RMP_HELO:
    case SETTINGS_RMP_READ_REQ:
    case SETTINGS_RMP_WRITE_REQ:
        break;
    }
}

static bool input_crsf_open(void *input, void *config)
{
    input_crsf_config_t *config_crsf = config;
    input_crsf_t *input_crsf = input;
    input_crsf->bps = CRSF_INPUT_BPS_DETECT;

    LOG_I(TAG, "Open on GPIO %s", gpio_toa(config_crsf->gpio));

    io_t crsf_io = {
        .read = NULL,
        .write = input_crsf_write,
        .data = input,
    };

    crsf_port_init(&input_crsf->crsf, &crsf_io, input_crsf_frame_callback, input);

    serial_port_config_t serial_config = {
        .baud_rate = CRSF_OPENTX_BAUDRATE,
        .tx_pin = config_crsf->gpio,
        .rx_pin = config_crsf->gpio,
        .tx_buffer_size = 128,
        .parity = SERIAL_PARITY_DISABLE,
        .stop_bits = SERIAL_STOP_BITS_1,
        .inverted = true,
        .byte_callback = input_crsf_isr,
        .byte_callback_data = input_crsf,
    };

    input_crsf->serial_port = serial_port_open(&serial_config);
    input_crsf->baud_rate = serial_config.baud_rate;

    time_micros_t now = time_micros_now();
    input_crsf->telemetry_pos = 0;
    input_crsf->last_byte_at = 0;
    input_crsf->last_frame_recv = now;
    input_crsf->next_resp_frame = TIME_MICROS_MAX;
    input_crsf->enable_rx_deadline = TIME_MICROS_MAX;
    input_crsf->bps_detect_switched = now;
    input_crsf->gpio = config_crsf->gpio;

    // 100ms should be low enough to be detected fast and high enough that
    // it's not accidentally triggered in 115200bps mode.
    failsafe_set_max_interval(&input_crsf->input.failsafe, MILLIS_TO_MICROS(100));
    failsafe_reset_interval(&input_crsf->input.failsafe, now);

    msp_telemetry_init_input(&input_crsf->msp_telemetry, CRSF_MSP_RESP_PAYLOAD_SIZE);
    INPUT_SET_MSP_TRANSPORT(input_crsf, MSP_TRANSPORT(&input_crsf->msp_telemetry));

    input_crsf->rmp_port = rmp_open_port(input_crsf->input.rc_data->rmp, 0, input_crsf_rmp_handler, input_crsf);
    memset(input_crsf->rmp_addresses, 0, sizeof(input_crsf->rmp_addresses));
    input_crsf->ping_filter = 0;

    return true;
}

static bool input_crsf_update(void *input, rc_data_t *data, time_micros_t now)
{
    input_crsf_t *input_crsf = input;
    bool updated = false;
    if (input_crsf->last_byte_at > 0 && now > input_crsf->last_byte_at && (now - input_crsf->last_byte_at) > input_crsf_tx_done_timeout_us(input_crsf))
    {
        // Transmission from radio has ended. We either got a frame
        // or corrupted data.
        if (crsf_port_decode(&input_crsf->crsf))
        {
            failsafe_reset_interval(&input_crsf->input.failsafe, now);
            updated = true;

            if (input_crsf->bps == CRSF_INPUT_BPS_DETECT)
            {
                LOG_I(TAG, "Detected baud rate %u", input_crsf->baud_rate);
                switch (input_crsf->baud_rate)
                {
                case CRSF_OPENTX_BAUDRATE:
                    input_crsf->bps = CRSF_INPUT_BPS_400K;
                    break;
                case CRSF_OPENTX_SLOW_BAUDRATE:
                    input_crsf->bps = CRSF_INPUT_BPS_115K;
                    break;
                default:
                    LOG_E(TAG, "Invalid baud rate %u", input_crsf->baud_rate);
                    UNREACHABLE();
                }
            }

            // If we decode a request, send a response (either an scheduled one
            // or a telemetry frame). Note that if we take more than 1s to resend the
            // CRSF_FRAMETYPE_LINK_STATISTICS frame, it will cause an RSSI lost warning
            // in the radio.
            input_crsf->next_resp_frame = now + RESPONSE_WAIT_INTERVAL_US;
        }
        // XXX: For now, we always reset the CRSF decoder since sometimes
        // when using RMP we might end up with several frames in the queue.
        crsf_port_reset(&input_crsf->crsf);

        input_crsf->last_byte_at = 0;
    }
    if (input_crsf->last_frame_recv + BPS_FALLBACK_INTERVAL < now && input_crsf->bps != CRSF_INPUT_BPS_DETECT)
    {
        LOG_D(TAG, "Enable bps detection");
        // If the radio doesn't respond, cancel telemetry send (if any)
        // and fallback to autodetection. Don't use failsafe for detection
        // since it might require
        input_crsf->next_resp_frame = TIME_MICROS_MAX;
        input_crsf->bps = CRSF_INPUT_BPS_DETECT;
        input_crsf->bps_detect_switched = now;
    }
    if (now > input_crsf->next_resp_frame)
    {
        input_crsf->enable_rx_deadline = now + input_crsf_max_tx_time_us(input_crsf);
        input_crsf->next_resp_frame = TIME_MICROS_MAX;
        input_crsf_send_response(input_crsf);
    }
    // The TX_DONE interrupt won't fire if there's a collision, so we use a timer
    // as a fallback to avoid leaving the pin in the TX state.
    if (now > input_crsf->enable_rx_deadline)
    {
        if (serial_port_half_duplex_mode(input_crsf->serial_port) == SERIAL_HALF_DUPLEX_MODE_TX)
        {
            serial_port_set_half_duplex_mode(input_crsf->serial_port, SERIAL_HALF_DUPLEX_MODE_RX);
        }
        input_crsf->enable_rx_deadline = TIME_MICROS_MAX;
    }
    if (input_crsf->bps == CRSF_INPUT_BPS_DETECT && input_crsf->bps_detect_switched + BPS_DETECT_SWITCH_INTERVAL_US < now)
    {
        switch (input_crsf->baud_rate)
        {
        case CRSF_OPENTX_BAUDRATE:
            input_crsf_update_baud_rate(input_crsf, CRSF_OPENTX_SLOW_BAUDRATE);
            break;
        case CRSF_OPENTX_SLOW_BAUDRATE:
            input_crsf_update_baud_rate(input_crsf, CRSF_OPENTX_BAUDRATE);
            break;
        default:
            LOG_E(TAG, "Invalid baud rate %u", input_crsf->baud_rate);
            UNREACHABLE();
        }
        crsf_port_reset(&input_crsf->crsf);
        input_crsf->bps_detect_switched = now;
    }
    return updated;
}

static void input_crsf_close(void *input, void *config)
{
    input_crsf_t *input_crsf = input;
    if (input_crsf->rmp_port)
    {
        rmp_close_port(input_crsf->input.rc_data->rmp, input_crsf->rmp_port);
        input_crsf->rmp_port = NULL;
    }
    serial_port_destroy(&input_crsf->serial_port);
}

void input_crsf_init(input_crsf_t *input)
{
    input->serial_port = NULL;
    input->input.vtable = (input_vtable_t){
        .open = input_crsf_open,
        .update = input_crsf_update,
        .close = input_crsf_close,
    };
    RING_BUFFER_INIT(&input->scheduled, crsf_frame_t, CRSF_INPUT_FRAME_QUEUE_SIZE);
}
