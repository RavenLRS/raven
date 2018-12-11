#include <stdint.h>
#include <string.h>

#include "msp/msp.h"

#include "util/macros.h"

#include "output_msp.h"

#define MSP_RC_MAX_SUPPORTED_CHANNELS 18
// No data in the response
#define MSP_SET_RAW_RC_RESPONSE_LENGTH MSP_V1_PROTOCOL_BYTES

#define MSP_SEND_REQ(output, req) OUTPUT_MSP_SEND_REQ(output, req, output_msp_message_callback)

#define MSP_POLL_INTERVAL_NONE 0
#define MSP_POLL_INTERVAL_SLOW SECS_TO_MICROS(10)
#define MSP_POLL_INTERVAL_NORMAL FREQ_TO_MICROS(2)
#define MSP_POLL_INTERVAL_FAST FREQ_TO_MICROS(4)
#define MSP_POLL_NONE(code) ((output_msp_poll_t){code, MSP_POLL_INTERVAL_NONE, 0})
#define MSP_POLL_SLOW(code) ((output_msp_poll_t){code, MSP_POLL_INTERVAL_SLOW, 0})
#define MSP_POLL_NORMAL(code) ((output_msp_poll_t){code, MSP_POLL_INTERVAL_NORMAL, 0})
#define MSP_POLL_FAST(code) ((output_msp_poll_t){code, MSP_POLL_INTERVAL_FAST, 0})

typedef struct msp_channels_payload_s
{
    uint16_t channels[MSP_RC_MAX_SUPPORTED_CHANNELS];
} msp_channels_payload_t;

static uint16_t msp_value_from_channel(unsigned value)
{
    /*
     MIN: 172 => 988
     MID: 992 => 1500
     MAX: 1811 => 2012
     scale = (2012-988) / (1811-172) = 0.62477120195241
     offset = 988 - 172 * 0.62477120195241 = 880.53935326418548
    */
    return (0.62477120195241f * value) + 881;
}

typedef enum
{
    GPS_NO_FIX = 0,
    GPS_FIX_2D,
    GPS_FIX_3D
} msp_gps_fix_type_e;

typedef struct msp_raw_gps_s
{
    uint8_t fix_type;       // msp_gps_fix_type_e
    uint8_t num_sats;       // actual number
    uint32_t lat;           // degree / 10`000`000
    uint32_t lon;           // same as lat
    uint16_t alt;           // meters
    uint16_t speed;         // cm/s
    uint16_t ground_course; // 0.1 degrees
    uint16_t hdop;          // 0.01
} PACKED msp_raw_gps_t;

typedef struct msp_analog_s
{
    uint8_t vbat; // battery voltage in 0.1V
    uint16_t mah_drawn;
    uint16_t rssi;
    // Can change depending on batteryConfig()->multiwiiCurrentMeterOutput
    // If true: multiwii_current = amperage in 0.001 A steps. Negative range is truncated to zero
    // If false: current = amperage in 0.01 A steps, range is -320A to 320A
    union {
        uint16_t multiwii_current;
        int16_t current;
    };
} PACKED msp_analog_t;

typedef struct msp_current_meter_config_s
{
    uint16_t current_meter_scale;
    uint16_t current_meter_offset;
    uint8_t current_meter_type;
    uint16_t battery_capacity; // mah
} PACKED msp_current_meter_config_t;

typedef struct msp_altitude_s
{
    int32_t altitude;       // cm
    int16_t vertical_speed; // cm/s
} PACKED msp_altitude_t;

typedef struct msp_attitude_s
{
    uint16_t roll;  // 0.1deg
    uint16_t pitch; // 0.1deg
    uint16_t yaw;   // 1deg!!!! - NOTE THE DIFFERENCE [0, 360)
} PACKED msp_attitude_t;

typedef struct msp_raw_imu_s
{
    int16_t acc_x; // acc in 1G * 512
    int16_t acc_y;
    int16_t acc_z;
    uint16_t gyro_x; // rotation in DPS
    uint16_t gyro_y;
    uint16_t gyro_z;
    uint16_t mag_x;
    uint16_t mag_y;
    uint16_t mag_z;
} PACKED msp_raw_imu_t;

typedef struct msp_misc_s
{
    uint16_t mid_rc;

    uint16_t min_throttle;
    uint16_t max_throttle;
    uint16_t min_command;

    uint16_t failsafe_throttle;

    uint8_t gps_provider;
    uint8_t gps_baudrate;
    uint8_t gps_sbas_mode;

    uint8_t multiwii_current_meter_output;
    uint8_t rssi_channel;
    uint8_t ignore1;

    uint16_t mag_declination;

    uint8_t vbat_scale;
    uint8_t vbat_min_cell_voltage;
    uint8_t vbat_max_cell_voltage;
    uint8_t vbat_warning_cell_voltage;
} PACKED msp_misc_t;

static uint32_t output_msp_acc_to_telemetry(int16_t acc)
{
    // Convert from (1G * 512) to 0.01G
    return acc * (100 / 512.0f);
}

static void output_msp_message_callback(msp_conn_t *conn, uint16_t cmd, const void *payload, int size, void *arg)
{
    switch (cmd)
    {
    case MSP_RAW_GPS:
    {
        if (size < sizeof(msp_raw_gps_t))
        {
            break;
        }
        const msp_raw_gps_t *data = payload;
        uint8_t fix_type = 0;
        switch ((msp_gps_fix_type_e)data->fix_type)
        {
        case GPS_NO_FIX:
            fix_type = TELEMETRY_GPS_FIX_NONE;
            break;
        case GPS_FIX_2D:
            fix_type = TELEMETRY_GPS_FIX_2D;
            break;
        case GPS_FIX_3D:
            fix_type = TELEMETRY_GPS_FIX_3D;
            break;
        }
        OUTPUT_TELEMETRY_UPDATE_U8(arg, TELEMETRY_ID_GPS_FIX, fix_type);
        OUTPUT_TELEMETRY_UPDATE_U8(arg, TELEMETRY_ID_GPS_NUM_SATS, data->num_sats);
        OUTPUT_TELEMETRY_UPDATE_I32(arg, TELEMETRY_ID_GPS_LAT, data->lat);
        OUTPUT_TELEMETRY_UPDATE_I32(arg, TELEMETRY_ID_GPS_LON, data->lon);
        // MSP_RAW_GPS sends meters, we want cm
        OUTPUT_TELEMETRY_UPDATE_I32(arg, TELEMETRY_ID_GPS_ALT, data->alt * 100);
        OUTPUT_TELEMETRY_UPDATE_U16(arg, TELEMETRY_ID_GPS_SPEED, data->speed);
        // ground course is in 0.1deg, we want 0.01deg
        OUTPUT_TELEMETRY_UPDATE_U16(arg, TELEMETRY_ID_GPS_HEADING, data->ground_course * 10);
        OUTPUT_TELEMETRY_UPDATE_U16(arg, TELEMETRY_ID_GPS_HDOP, data->hdop);
        break;
    }
    case MSP_ANALOG:
    {
        if (size < sizeof(msp_analog_t))
        {
            break;
        }
        const msp_analog_t *data = payload;
        // vbat comes in 0.1V, we want 0.01V
        OUTPUT_TELEMETRY_UPDATE_U16(arg, TELEMETRY_ID_BAT_VOLTAGE, data->vbat * 10);
        // current drawn comes in mah
        OUTPUT_TELEMETRY_UPDATE_I32(arg, TELEMETRY_ID_CURRENT_DRAWN, data->mah_drawn);
        // Since MSP doesn't support battery percentage, calculate it here if
        // we have battery capacity.
        OUTPUT_TELEMETRY_CALCULATE(arg, TELEMETRY_ID_BAT_REMAINING_P);
        if (((output_msp_t *)arg)->multiwii_current_meter_output)
        {
            // multiwii style, 0.001A
            OUTPUT_TELEMETRY_UPDATE_I16(arg, TELEMETRY_ID_CURRENT, data->current / 10);
        }
        else
        {
            // not multiwii, 0.01 A
            OUTPUT_TELEMETRY_UPDATE_I16(arg, TELEMETRY_ID_CURRENT, data->current);
        }
        break;
    }
    case MSP_CURRENT_METER_CONFIG:
    {
        if (size < sizeof(msp_current_meter_config_t))
        {
            break;
        }
        const msp_current_meter_config_t *data = payload;
        OUTPUT_TELEMETRY_UPDATE_U16(arg, TELEMETRY_ID_BAT_CAPACITY, data->battery_capacity);
        break;
    }
    case MSP_ALTITUDE:
    {
        if (size < sizeof(msp_altitude_t))
        {
            break;
        }
        const msp_altitude_t *data = payload;
        OUTPUT_TELEMETRY_UPDATE_I32(arg, TELEMETRY_ID_ALTITUDE, data->altitude);
        OUTPUT_TELEMETRY_UPDATE_I16(arg, TELEMETRY_ID_VERTICAL_SPEED, data->vertical_speed);
        break;
    }
    case MSP_ATTITUDE:
    {
        if (size < sizeof(msp_attitude_t))
        {
            break;
        }
        const msp_attitude_t *data = payload;
        OUTPUT_TELEMETRY_UPDATE_I16(arg, TELEMETRY_ID_ATTITUDE_X, data->pitch * 10);
        OUTPUT_TELEMETRY_UPDATE_I16(arg, TELEMETRY_ID_ATTITUDE_Y, data->roll * 10);
        // Convert from (0, 360] to (-180, 180)
        OUTPUT_TELEMETRY_UPDATE_I16(arg, TELEMETRY_ID_ATTITUDE_Z, (data->yaw > 180 ? -360 + data->yaw : data->yaw) * 100);
        OUTPUT_TELEMETRY_UPDATE_U16(arg, TELEMETRY_ID_HEADING, data->yaw * 100);
        break;
    }
    case MSP_RAW_IMU:
    {
        if (size < sizeof(msp_raw_imu_t))
        {
            break;
        }
        const msp_raw_imu_t *data = payload;
        // TODO: Units are probably wrong here
        OUTPUT_TELEMETRY_UPDATE_I32(arg, TELEMETRY_ID_ACC_X, output_msp_acc_to_telemetry(data->acc_x));
        OUTPUT_TELEMETRY_UPDATE_I32(arg, TELEMETRY_ID_ACC_Y, output_msp_acc_to_telemetry(data->acc_y));
        OUTPUT_TELEMETRY_UPDATE_I32(arg, TELEMETRY_ID_ACC_Z, output_msp_acc_to_telemetry(data->acc_z));
        break;
    }
    case MSP_MISC:
        if (size < sizeof(msp_misc_t))
        {
            break;
        }
        const msp_misc_t *data = payload;
        ((output_msp_t *)arg)->multiwii_current_meter_output = data->multiwii_current_meter_output;
        break;
    }
}

static bool output_msp_open(void *output, void *config)
{
    output_msp_t *output_msp = output;
    output_msp_config_t *cfg = config;

    serial_port_config_t serial_config = {
        .baud_rate = msp_serial_baudrate_get(cfg->baud_rate),
        .tx_pin = cfg->tx,
        .rx_pin = cfg->rx,
        .tx_buffer_size = 256,
        .rx_buffer_size = MSP_MAX_PAYLOAD_SIZE * 2,
        .parity = SERIAL_PARITY_DISABLE,
        .stop_bits = SERIAL_STOP_BITS_1,
        .inverted = false,
    };

    output_msp->output.serial_port = serial_port_open(&serial_config);
    if (serial_port_is_half_duplex(output_msp->output.serial_port))
    {
        // With half duplex at 115200 bps we're limited to 50hz
        output_msp->output.min_rc_update_interval = FREQ_TO_MICROS(50);
    }
    else
    {
        output_msp->output.min_rc_update_interval = 0;
    }
    io_t msp_serial_io = SERIAL_IO(output_msp->output.serial_port);
    msp_serial_init(&output_msp->msp_serial, &msp_serial_io);
    OUTPUT_SET_MSP_TRANSPORT(output_msp, MSP_TRANSPORT(&output_msp->msp_serial));
    return true;
}

static bool output_msp_update(void *output, rc_data_t *data, bool update_rc, time_micros_t now)
{
    output_msp_t *output_msp = output;
    // RC via MSP is implemented in fc_msp.c, via the MSP_SET_RAW_RC code.
    // Each channel is received as an uint16_t and its value must be the actual
    // PWM us channel value (i.e. [1000-2000] range). A request can contain
    // an arbitrary number of channels up to MAX_SUPPORTED_RC_CHANNEL_COUNT
    // which is 18 right now in iNav. Note that a request with more than
    // this number of channels is rejected. Channels not present in a request
    // are set to zero. There are no flags, failsafe is detected by a timeout
    // in the updates.
    bool updated = false;
    if (update_rc)
    {
        msp_channels_payload_t payload;
        for (int ii = 0; ii < MSP_RC_MAX_SUPPORTED_CHANNELS; ii++)
        {
            if (ii < RC_CHANNELS_NUM)
            {
                control_channel_t *ch = &data->channels[ii];
                payload.channels[ii] = msp_value_from_channel(ch->value);
            }
            else
            {
                payload.channels[ii] = 1000;
            }
        }
        if (msp_conn_send(OUTPUT_MSP_CONN_GET(output), MSP_SET_RAW_RC, &payload, sizeof(payload), NULL, NULL) > 0)
        {
            updated = true;
        }
    }
    // Check if we need to poll for any telemetry
    for (int ii = 0; ii < OUTPUT_MSP_POLL_COUNT; ii++)
    {
        if (output_msp->polls[ii].interval == MSP_POLL_INTERVAL_NONE)
        {
            continue;
        }
        if (output_msp->polls[ii].next_poll < now)
        {
            if (MSP_SEND_REQ(output, output_msp->polls[ii].cmd) > 0)
            {
                output_msp->polls[ii].next_poll = now + output_msp->polls[ii].interval;
            }
        }
    }
    return updated;
}

static void output_msp_close(void *out, void *config)
{
    output_t *output = out;
    serial_port_destroy(&output->serial_port);
}

void output_msp_init(output_msp_t *output)
{
    // XXX: Only iNAV 1.9+ will work with updates every
    // 10ms. For earliear versions the faster we can do
    // is 150ms due to a bug in the MSP out of band data
    // handling (this is at 115200)
    // Betaflight hasn't been tested yet.
    output->output.flags = OUTPUT_FLAG_LOCAL;
    output->output.vtable = (output_vtable_t){
        .open = output_msp_open,
        .update = output_msp_update,
        .close = output_msp_close,
    };

    output_msp_poll_t polls[] = {
        // TELEMETRY_ID_BAT_VOLTAGE, TELEMETRY_ID_CURRENT, TELEMETRY_ID_CURRENT_DRAWN
        MSP_POLL_NORMAL(MSP_ANALOG),
        // TELEMETRY_ID_BAT_CAPACITY
        MSP_POLL_SLOW(MSP_CURRENT_METER_CONFIG),
        // TELEMETRY_ID_ALTITUDE, TELEMETRY_ID_VERTICAL_SPEED
        MSP_POLL_NORMAL(MSP_ALTITUDE),
        // TELEMETRY_ID_HEADING
        MSP_POLL_FAST(MSP_ATTITUDE),
        // TELEMETRY_ID_ACC_X, TELEMETRY_ID_ACC_Y, TELEMETRY_ID_ACC_Z
        MSP_POLL_FAST(MSP_RAW_IMU),
        // TELEMETRY_ID_GPS_FIX, TELEMETRY_ID_GPS_NUM_SATS, TELEMETRY_ID_GPS_LAT,
        // TELEMETRY_ID_GPS_LON, TELEMETRY_ID_GPS_ALT, TELEMETRY_ID_GPS_SPEED
        // TELEMETRY_ID_GPS_HEADING, TELEMETRY_ID_GPS_HDOP
        MSP_POLL_NORMAL(MSP_RAW_GPS),
        // Used to retrieve RSSI output channel and wheter current meter
        // output is multiwii style
        MSP_POLL_SLOW(MSP_MISC),
    };
    // Calculated: TELEMETRY_ID_BAT_REMAINING_P
    // TODO: TELEMETRY_ID_AVG_CELL_VOLTAGE, TELEMETRY_ID_ATTITUDE_*
    ARRAY_ASSERT_COUNT(polls, OUTPUT_MSP_POLL_COUNT, "invalid OUTPUT_MSP_POLL_COUNT");
    memcpy(output->polls, polls, sizeof(polls));

    // Assume false at first, since it's the default value
    output->multiwii_current_meter_output = false;
}
