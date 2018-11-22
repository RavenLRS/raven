#include <stdio.h>

#include <hal/log.h>

#include "config/settings_rmp.h"

#include "rc/rc_data.h"

#include "rmp/rmp.h"

#include "util/version.h"

#include "input_ibus.h"

static const char *TAG = "IBUS.Input";

#define BPS_DETECT_SWITCH_INTERVAL_US MILLIS_TO_MICROS(1000)
#define BPS_FALLBACK_INTERVAL SECS_TO_MICROS(1)
#define RESPONSE_WAIT_INTERVAL_US (500)

static uint16_t input_ibus_channel_value_mapping(uint16_t ch)
{
    return RC_CHANNEL_MIN_VALUE +
           (ch - IBUS_CHANNEL_VALUE_MIN) * (RC_CHANNEL_MAX_VALUE - RC_CHANNEL_MIN_VALUE) /
               (IBUS_CHANNEL_VALUE_MAX - IBUS_CHANNEL_VALUE_MIN);
}

static void input_ibus_isr(const serial_port_t *port, uint8_t b, void *user_data)
{
    input_ibus_t *input = user_data;
    // Received byte in TX mode
    time_micros_t now = time_micros_now();
    input->last_byte_at = now;

    ibus_port_t *ibusport = &input->ibus;

    if (ibusport->buf_pos < sizeof(ibusport->buf))
    {
        ibusport->buf[ibusport->buf_pos++] = b;
    }
}

//no recv after this us time indicates that a tx frame is done
static unsigned input_ibus_tx_done_timeout_us(input_ibus_t *input)
{
    return 2000;
}

static unsigned input_ibus_frame_interval_us(input_ibus_t *input)
{
    return 7000;
}

static void input_ibus_frame_callback(void *data, ibus_frame_t *frame)
{
    input_ibus_t *input_ibus = data;
    time_micros_t now = time_micros_now();
    switch (frame->payload.pack_type)
    {
    case IBUS_FRAMETYPE_RC_CHANNELS:
    {
        for (int i = 0; i < IBUS_NUM_CHANNELS; i++)
        {
            rc_data_update_channel(input_ibus->input.rc_data, i, 
                input_ibus_channel_value_mapping(frame->payload.ch[i]), now);
        }
        break;
    }
    default:
        LOG_W(TAG, "Unknown frame type 0x%02x with size %u", frame->payload.pack_type, frame->payload.pack_len);
        LOG_BUFFER_W(TAG, frame, frame->payload.pack_len);
    }

    // Note that we always should get a frame with the same period. When there's extra data from
    // the radio (e.g. an MSP request), it will replace control frames.
    unsigned frame_interval = input_ibus_frame_interval_us(input_ibus);
    if (frame_interval > 0)
    {
        if (input_ibus->last_frame_recv > 0 && now - input_ibus->last_frame_recv > frame_interval * 1.5f)
        {
            LOG_W(TAG, "Lost a frame, interval between 2 was %llu", now - input_ibus->last_frame_recv);
        }
    }
    input_ibus->last_frame_recv = now;
}

static bool input_ibus_open(void *input, void *config)
{
    input_ibus_config_t *config_ibus = config;
    input_ibus_t *input_ibus = input;
    input_ibus->bps = IBUS_INPUT_BPS_DETECT;

    LOG_I(TAG, "Open");

    io_t ibus_io = {
        .read = NULL,
        .write = NULL,
        .data = input,
    };

    ibus_port_init(&input_ibus->ibus, &ibus_io, input_ibus_frame_callback, input);

    serial_port_config_t serial_config = {
        .baud_rate = IBUS_BAUDRATE,
        .tx_pin = config_ibus->gpio,
        .rx_pin = config_ibus->gpio,
        .tx_buffer_size = 128,
        .parity = SERIAL_PARITY_DISABLE,
        .stop_bits = SERIAL_STOP_BITS_1,
        .inverted = false,
        .byte_callback = input_ibus_isr,
        .byte_callback_data = input_ibus,
    };

    input_ibus->serial_port = serial_port_open(&serial_config);
    input_ibus->baud_rate = serial_config.baud_rate;

    time_micros_t now = time_micros_now();
    input_ibus->telemetry_pos = 0;
    input_ibus->last_byte_at = 0;
    input_ibus->last_frame_recv = now;
    input_ibus->next_resp_frame = TIME_MICROS_MAX;
    input_ibus->enable_rx_deadline = TIME_MICROS_MAX;
    input_ibus->bps_detect_switched = now;
    input_ibus->gpio = config_ibus->gpio;

    // 100ms should be low enough to be detected fast and high enough that
    // it's not accidentally triggered in 115200bps mode.
    failsafe_set_max_interval(&input_ibus->input.failsafe, MILLIS_TO_MICROS(100));
    failsafe_reset_interval(&input_ibus->input.failsafe, now);

    return true;
}

static bool input_ibus_update(void *input, rc_data_t *data, time_micros_t now)
{
    input_ibus_t *input_ibus = input;
    bool updated = false;
    if (input_ibus->last_byte_at > 0 && now > input_ibus->last_byte_at && (now - input_ibus->last_byte_at) > input_ibus_tx_done_timeout_us(input_ibus))
    {
        // Transmission from radio has ended. We either got a frame
        // or corrupted data.
        if (ibus_port_decode(&input_ibus->ibus))
        {
            failsafe_reset_interval(&input_ibus->input.failsafe, now);
            updated = true;

            // If we decode a request, send a response (either an scheduled one
            // or a telemetry frame). Note that if we take more than 1s to resend the
            // IBUS_FRAMETYPE_LINK_STATISTICS frame, it will cause an RSSI lost warning
            // in the radio.
            input_ibus->next_resp_frame = now + RESPONSE_WAIT_INTERVAL_US;
        }
        // XXX: For now, we always reset the IBUS decoder since sometimes
        // when using RMP we might end up with several frames in the queue.
        ibus_port_reset(&input_ibus->ibus);

        input_ibus->last_byte_at = 0;
    }

    if (now > input_ibus->next_resp_frame)
    {
    }
    return updated;
}

static void input_ibus_close(void *input, void *config)
{
    input_ibus_t *input_ibus = input;
    if (input_ibus->rmp_port)
    {
        rmp_close_port(input_ibus->input.rc_data->rmp, input_ibus->rmp_port);
        input_ibus->rmp_port = NULL;
    }
    serial_port_destroy(&input_ibus->serial_port);
}

void input_ibus_init(input_ibus_t *input)
{
    input->serial_port = NULL;
    input->input.vtable = (input_vtable_t){
        .open = input_ibus_open,
        .update = input_ibus_update,
        .close = input_ibus_close,
    };
    RING_BUFFER_INIT(&input->scheduled, ibus_frame_t, IBUS_INPUT_FRAME_QUEUE_SIZE);
}
