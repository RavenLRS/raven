#include <math.h>
#include <string.h>

#include <hal/log.h>

#include "rc/rc_data.h"
#include "rc/telemetry.h"

#include "output_crsf.h"

#define CRSF_SERIAL_BUFFER_SIZE 256
#define CRSF_PING_INTERVAL SECS_TO_MICROS(10)
#define DECIKMH_TO_CMS(x) (x * (10.0f / 3.6f))

static const char *TAG = "CRSF.Output";

static int16_t crsf_dec_att_angle(int16_t val)
{
    return CRSF_DEC_I16(val) * (100 / (10000.0f * (M_PI / 180.0f)));
}

static uint16_t crsf_dec_att_heading(uint16_t val)
{
    return CRSF_DEC_U16(val) * (100 / (10000.0f * (M_PI / 180.0f)));
}

static int16_t crsf_dec_att_z(int16_t val)
{
    int32_t v = crsf_dec_att_heading(val);
    if (v > 180 * 100)
    {
        v -= 360 * 100;
    }
    return v;
}

static void output_crsf_frame_callback(void *data, crsf_frame_t *frame)
{
    output_crsf_t *output = data;
    switch (frame->header.type)
    {
    case CRSF_FRAMETYPE_GPS:
        OUTPUT_TELEMETRY_UPDATE_I32(output, TELEMETRY_ID_GPS_LAT, CRSF_DEC_I32(frame->gps.lat));
        OUTPUT_TELEMETRY_UPDATE_I32(output, TELEMETRY_ID_GPS_LON, CRSF_DEC_I32(frame->gps.lon));
        OUTPUT_TELEMETRY_UPDATE_U16(output, TELEMETRY_ID_GPS_SPEED, DECIKMH_TO_CMS(CRSF_DEC_U16(frame->gps.ground_speed)));
        OUTPUT_TELEMETRY_UPDATE_U16(output, TELEMETRY_ID_GPS_HEADING, CRSF_DEC_U16(frame->gps.heading));
        OUTPUT_TELEMETRY_UPDATE_I32(output, TELEMETRY_ID_ALTITUDE, (CRSF_DEC_U16(frame->gps.altitude) - 1000) * 100);
        OUTPUT_TELEMETRY_UPDATE_U8(output, TELEMETRY_ID_GPS_NUM_SATS, frame->gps.sats);
        break;
    case CRSF_FRAMETYPE_ATTITUDE:
        OUTPUT_TELEMETRY_UPDATE_I16(output, TELEMETRY_ID_ATTITUDE_X, crsf_dec_att_angle(frame->attitude.pitch));
        OUTPUT_TELEMETRY_UPDATE_I16(output, TELEMETRY_ID_ATTITUDE_Y, crsf_dec_att_angle(frame->attitude.roll));
        // Note that INAV/BF/CF send yaw as [0, 360), so we use that as TELEMETRY_ID_HEADING
        // and convert it to (-180, 180) for TELEMETRY_ID_ATTITUDE_Z. Be careful with overflow,
        // since yaw might be bigger than 2^15 - 1 and won't fit in an int16_t, causing wrapping.
        OUTPUT_TELEMETRY_UPDATE_I16(output, TELEMETRY_ID_ATTITUDE_Z, crsf_dec_att_z(frame->attitude.yaw));
        OUTPUT_TELEMETRY_UPDATE_U16(output, TELEMETRY_ID_HEADING, crsf_dec_att_heading(frame->attitude.yaw));
        break;
    case CRSF_FRAMETYPE_BATTERY_SENSOR:
        OUTPUT_TELEMETRY_UPDATE_U16(output, TELEMETRY_ID_BAT_VOLTAGE, CRSF_DEC_U16(frame->battery_sensor.voltage) * 10);
        OUTPUT_TELEMETRY_UPDATE_I16(output, TELEMETRY_ID_CURRENT, CRSF_DEC_U16(frame->battery_sensor.current) * 10);
        OUTPUT_TELEMETRY_UPDATE_I32(output, TELEMETRY_ID_CURRENT_DRAWN, CRSF_DEC_U24(frame->battery_sensor.mah_drawn));
        OUTPUT_TELEMETRY_UPDATE_U8(output, TELEMETRY_ID_BAT_REMAINING_P, frame->battery_sensor.percentage_remaining);
        break;
    case CRSF_FRAMETYPE_FLIGHT_MODE:
        OUTPUT_TELEMETRY_UPDATE_STRING(output, TELEMETRY_ID_FLIGHT_MODE_NAME, crsf_frame_str(frame), -1);
        break;
    case CRSF_FRAMETYPE_MSP_RESP:
    {
        crsf_ext_frame_t *msp_frame = crsf_frame_to_ext(frame);
        msp_telemetry_push_response_chunk(&output->msp_telemetry, msp_frame->msp.payload, crsf_ext_frame_payload_size(msp_frame));
        break;
    }
    case CRSF_FRAMETYPE_DEVICE_INFO:
    {
        crsf_ext_frame_t *ext_frame = crsf_frame_to_ext(frame);
        LOG_D(TAG, "Device name %s", ext_frame->device_info.name);
        break;
    }
    default:
        LOG_W(TAG, "Unknown frame type 0x%02X", frame->header.type);
        LOG_BUFFER_W(TAG, frame, frame->header.frame_size + CRSF_FRAME_NOT_COUNTED_BYTES);
    }
}

static void output_crsf_ping(output_crsf_t *output_crsf)
{
    crsf_frame_t frame = {
        .header = {
            .device_addr = CRSF_ADDRESS_BROADCAST,
            .frame_size = CRSF_FRAME_SIZE(0),
            .type = CRSF_FRAMETYPE_DEVICE_PING,
        },
    };
    crsf_port_write(&output_crsf->crsf, &frame);
}

static bool output_crsf_open(void *output, void *config)
{
    output_crsf_config_t *config_crsf = config;
    LOG_I(TAG, "Open with TX %d, RX %d", (int)config_crsf->tx_pin_num, (int)config_crsf->rx_pin_num);
    output_crsf_t *output_crsf = output;

    bool half_duplex = config_crsf->tx_pin_num == config_crsf->rx_pin_num;

    serial_port_config_t serial_config = {
        .baud_rate = CRSF_RX_BAUDRATE,
        .tx_pin = config_crsf->tx_pin_num,
        .rx_pin = config_crsf->rx_pin_num,
        .tx_buffer_size = half_duplex ? 0 : CRSF_SERIAL_BUFFER_SIZE,
        .rx_buffer_size = CRSF_SERIAL_BUFFER_SIZE,
        .parity = SERIAL_PARITY_DISABLE,
        .stop_bits = SERIAL_STOP_BITS_1,
        .inverted = config_crsf->inverted,
    };

    output_crsf->output.serial_port = serial_port_open(&serial_config);
    output_crsf->next_ping = 0;
    io_t crsf_port_io = SERIAL_IO(output_crsf->output.serial_port);
    crsf_port_init(&output_crsf->crsf, &crsf_port_io, output_crsf_frame_callback, output);
    msp_telemetry_init_output(&output_crsf->msp_telemetry, CRSF_MSP_REQ_PAYLOAD_SIZE);
    OUTPUT_SET_MSP_TRANSPORT(output_crsf, MSP_TRANSPORT(&output_crsf->msp_telemetry));
    return true;
}

static bool output_crsf_update(void *output, rc_data_t *data, time_micros_t now)
{
    output_crsf_t *output_crsf = output;
    crsf_port_read(&output_crsf->crsf);
    if (!rc_data_is_ready(data))
    {
        return false;
    }
    if (failsafe_is_active(data->failsafe.input))
    {
        // CRSF doesn't have any way to signal FS, so we just stop
        // sending frames to the FC
        return false;
    }
#define CH_TO_CRFS(ch) channel_to_crsf_value(data->channels[ch].value)
    crsf_frame_t frame = {
        .header = {
            .device_addr = CRSF_ADDRESS_BROADCAST,
            .frame_size = CRSF_FRAME_SIZE(sizeof(crsf_channels_t)),
            .type = CRSF_FRAMETYPE_RC_CHANNELS_PACKED,
        },
        .channels = {
            .ch0 = CH_TO_CRFS(0),
            .ch1 = CH_TO_CRFS(1),
            .ch2 = CH_TO_CRFS(2),
            .ch3 = CH_TO_CRFS(3),
            .ch4 = CH_TO_CRFS(4),
            .ch5 = CH_TO_CRFS(5),
            .ch6 = CH_TO_CRFS(6),
            .ch7 = CH_TO_CRFS(7),
            .ch8 = CH_TO_CRFS(8),
            .ch9 = CH_TO_CRFS(9),
            .ch10 = CH_TO_CRFS(10),
            .ch11 = CH_TO_CRFS(11),
            .ch12 = CH_TO_CRFS(12),
            .ch13 = CH_TO_CRFS(13),
            .ch14 = CH_TO_CRFS(14),
            .ch15 = CH_TO_CRFS(15),
        },
    };
    crsf_port_write(&output_crsf->crsf, &frame);
    if (output_crsf->next_ping < now)
    {
        output_crsf_ping(output_crsf);
        output_crsf->next_ping = now + CRSF_PING_INTERVAL;
    }
    else
    {
        crsf_ext_frame_t msp_frame;
        size_t msp_frame_size;
        if ((msp_frame_size = msp_telemetry_pop_request_chunk(&output_crsf->msp_telemetry, &msp_frame.msp.payload)) > 0)
        {
            if (msp_frame_size < CRSF_MSP_REQ_PAYLOAD_SIZE)
            {
                // Zero the rest
                memset(&msp_frame.msp.payload[msp_frame_size], 0, CRSF_MSP_REQ_PAYLOAD_SIZE - msp_frame_size);
            }
            LOG_D(TAG, "MSP request data");
            LOG_BUFFER_D(TAG, msp_frame.msp.payload, CRSF_MSP_REQ_PAYLOAD_SIZE);
            msp_frame.header.device_addr = CRSF_ADDRESS_BROADCAST,
            msp_frame.header.frame_size = CRSF_EXT_FRAME_SIZE(CRSF_MSP_REQ_PAYLOAD_SIZE);
            msp_frame.header.type = CRSF_FRAMETYPE_MSP_REQ;
            msp_frame.header.dest_addr = CRSF_ADDRESS_FLIGHT_CONTROLLER;
            msp_frame.header.orig_addr = CRSF_ADDRESS_RADIO_TRANSMITTER;
            crsf_port_write(&output_crsf->crsf, crsf_ext_frame_to_frame(&msp_frame));
        }
    }
    // TODO: Send stats: RSSI, etc...
    return true;
}

static void output_crsf_close(void *out, void *config)
{
    output_t *output = out;
    serial_port_destroy(&output->serial_port);
}

void output_crsf_init(output_crsf_t *output)
{
    output->output.min_update_interval = FREQ_TO_MICROS(250),
    output->output.flags = OUTPUT_FLAG_LOCAL;
    output->output.vtable = (output_vtable_t){
        .open = output_crsf_open,
        .update = output_crsf_update,
        .close = output_crsf_close,
    };
}
