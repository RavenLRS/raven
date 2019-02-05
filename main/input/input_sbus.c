#include <hal/log.h>

#include "input/input_sbus.h"

#include "protocols/sbus.h"

#include "rc/rc_data.h"

#define SBUS_INPUT_INVERSION_SWITCH_INTERVAL_US MILLIS_TO_MICROS(300)

static const char *TAG = "SBUS.Input";

static bool input_sbus_payload_is_valid(input_sbus_t *input)
{
    // If the FS bit is set, ignore the frame
    // TODO: Read the FS config and apply it
    if ((input->frame.payload.data.flags & SBUS_FLAG_FAILSAFE_ACTIVE) == 0)
    {
        // Since SBUS doesn't have a checksum, we require the boolean channel
        // flags to be zero as a poor man's error check.
        if ((input->frame.payload.data.flags & SBUS_FLAG_CHANNEL_16) == 0 &&
            (input->frame.payload.data.flags & SBUS_FLAG_CHANNEL_17) == 0)
        {
            // Ensure all channel values are within the valid range
#define SBUS_ENSURE_CH(n)                                               \
    do                                                                  \
    {                                                                   \
        if (input->frame.payload.data.ch##n < SBUS_CHANNEL_VALUE_MIN || \
            input->frame.payload.data.ch##n > SBUS_CHANNEL_VALUE_MAX)   \
        {                                                               \
            return false;                                               \
        }                                                               \
    } while (0)

            SBUS_ENSURE_CH(0);
            SBUS_ENSURE_CH(1);
            SBUS_ENSURE_CH(2);
            SBUS_ENSURE_CH(3);
            SBUS_ENSURE_CH(4);
            SBUS_ENSURE_CH(5);
            SBUS_ENSURE_CH(6);
            SBUS_ENSURE_CH(7);
            SBUS_ENSURE_CH(8);
            SBUS_ENSURE_CH(9);
            SBUS_ENSURE_CH(10);
            SBUS_ENSURE_CH(11);
            SBUS_ENSURE_CH(12);
            SBUS_ENSURE_CH(13);
            SBUS_ENSURE_CH(14);
            SBUS_ENSURE_CH(15);
#undef SBUS_ENSURE_CH

            return true;
        }
    }
    return false;
}

static void input_sbus_reset_inversion_switch(input_sbus_t *input, time_micros_t now)
{
    input->next_inversion_switch = now + SBUS_INPUT_INVERSION_SWITCH_INTERVAL_US;
}

static bool input_sbus_open(void *input, void *config)
{
    input_sbus_config_t *config_sbus = config;
    input_sbus_t *input_sbus = input;

    LOG_I(TAG, "Open RX: %s", gpio_toa(config_sbus->rx));

    // Default to "normal" SBUS, which is inverted
    input_sbus->inverted = true;

    serial_port_config_t serial_config = {
        .baud_rate = SBUS_BAUDRATE,
        .tx_pin = SERIAL_UNUSED_GPIO,
        .rx_pin = config_sbus->rx,
        .tx_buffer_size = 0,
        .rx_buffer_size = 0,
        .parity = SERIAL_PARITY_EVEN,
        .stop_bits = SERIAL_STOP_BITS_2,
        .inverted = input_sbus->inverted,
    };

    input_sbus->serial_port = serial_port_open(&serial_config);
    input_sbus->frame_pos = 0;
    input_sbus->frame_end = SBUS_EXPECTED_TRANSMISSION_TIME_US;

    time_micros_t now = time_micros_now();

    input_sbus_reset_inversion_switch(input_sbus, now);

    failsafe_set_max_interval(&input_sbus->input.failsafe, MILLIS_TO_MICROS(100));
    failsafe_reset_interval(&input_sbus->input.failsafe, now);
    return true;
}

static bool input_sbus_update(void *input, rc_data_t *data, time_micros_t now)
{
    input_sbus_t *input_sbus = input;
    if (now > input_sbus->frame_end)
    {
        input_sbus->frame_pos = 0;
        input_sbus->frame_end = TIME_MICROS_MAX;
    }
    if (failsafe_is_active(&input_sbus->input.failsafe))
    {
        if (input_sbus->next_inversion_switch < now)
        {
            input_sbus_reset_inversion_switch(input_sbus, now);
            input_sbus->inverted = !input_sbus->inverted;
            serial_port_set_inverted(input_sbus->serial_port, input_sbus->inverted);
        }
    }
    size_t rem = sizeof(input_sbus->frame.bytes) - input_sbus->frame_pos;
    int n = serial_port_read(input_sbus->serial_port, &input_sbus->frame.bytes[input_sbus->frame_pos], rem, 0);
    if (n > 0)
    {
        if (input_sbus->frame_pos == 0)
        {
            input_sbus->frame_end = now + (SBUS_EXPECTED_TRANSMISSION_TIME_US * 1.5f);
        }
        input_sbus->frame_pos += n;
        if (input_sbus->frame_pos == sizeof(input_sbus->frame.bytes))
        {
            input_sbus->frame_pos = 0;
            input_sbus->frame_end = TIME_MICROS_MAX;

            if (input_sbus_payload_is_valid(input_sbus))
            {
                rc_data_update_channel(data, 0, channel_from_sbus_value(input_sbus->frame.payload.data.ch0), now);
                rc_data_update_channel(data, 1, channel_from_sbus_value(input_sbus->frame.payload.data.ch1), now);
                rc_data_update_channel(data, 2, channel_from_sbus_value(input_sbus->frame.payload.data.ch2), now);
                rc_data_update_channel(data, 3, channel_from_sbus_value(input_sbus->frame.payload.data.ch3), now);
                rc_data_update_channel(data, 4, channel_from_sbus_value(input_sbus->frame.payload.data.ch4), now);
                rc_data_update_channel(data, 5, channel_from_sbus_value(input_sbus->frame.payload.data.ch5), now);
                rc_data_update_channel(data, 6, channel_from_sbus_value(input_sbus->frame.payload.data.ch6), now);
                rc_data_update_channel(data, 7, channel_from_sbus_value(input_sbus->frame.payload.data.ch7), now);
                rc_data_update_channel(data, 8, channel_from_sbus_value(input_sbus->frame.payload.data.ch8), now);
                rc_data_update_channel(data, 9, channel_from_sbus_value(input_sbus->frame.payload.data.ch9), now);
                rc_data_update_channel(data, 10, channel_from_sbus_value(input_sbus->frame.payload.data.ch10), now);
                rc_data_update_channel(data, 11, channel_from_sbus_value(input_sbus->frame.payload.data.ch11), now);
                rc_data_update_channel(data, 12, channel_from_sbus_value(input_sbus->frame.payload.data.ch12), now);
                rc_data_update_channel(data, 13, channel_from_sbus_value(input_sbus->frame.payload.data.ch13), now);
                rc_data_update_channel(data, 14, channel_from_sbus_value(input_sbus->frame.payload.data.ch14), now);
                rc_data_update_channel(data, 15, channel_from_sbus_value(input_sbus->frame.payload.data.ch15), now);
                rc_data_update_channel(data, 16, input_sbus->frame.payload.data.flags & SBUS_FLAG_CHANNEL_16 ? RC_CHANNEL_MAX_VALUE : RC_CHANNEL_MIN_VALUE, now);
                rc_data_update_channel(data, 17, input_sbus->frame.payload.data.flags & SBUS_FLAG_CHANNEL_17 ? RC_CHANNEL_MAX_VALUE : RC_CHANNEL_MIN_VALUE, now);

                failsafe_reset_interval(&input_sbus->input.failsafe, now);

                return true;
            }
        }
    }
    return false;
}

static void input_sbus_close(void *input, void *config)
{
    input_sbus_t *input_sbus = input;
    serial_port_destroy(&input_sbus->serial_port);
}

void input_sbus_init(input_sbus_t *input)
{
    input->serial_port = NULL;
    input->input.vtable = (input_vtable_t){
        .open = input_sbus_open,
        .update = input_sbus_update,
        .close = input_sbus_close,
    };
}
