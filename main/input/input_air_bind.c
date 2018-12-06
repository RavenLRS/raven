#include <hal/log.h>

#include "air/air_radio.h"

#include "rc/rc_data.h"

#include "ui/led.h"

#include "input_air_bind.h"

static const char *TAG = "Input.Air.Bind";

// Switch band every 2 seconds unless we're seing a TX in bind mode
#define BAND_SWITCH_INTERVAL_US MILLIS_TO_MICROS(2000)

enum
{
    AIR_INPUT_BIND_STATE_RX,
    AIR_INPUT_BIND_STATE_TX,
};

static bool input_air_bind_update_band(input_air_bind_t *input)
{
    air_band_e band = air_band_mask_get_band(input->air_config.bands, input->band_index);
    if (band == AIR_BAND_INVALID)
    {
        // No more bands, go back to first
        if (input->band_index == 0)
        {
            // No valid bands
            return false;
        }
        input->band_index = 0;
        band = air_band_mask_get_band(input->air_config.bands, input->band_index);
    }
    input->air_config.band = band;
    air_radio_set_frequency(input->air_config.radio, air_band_frequency(band), 0);
    return true;
}

static bool input_air_bind_open(void *data, void *config)
{
    LOG_I(TAG, "Open");
    input_air_bind_t *input = data;
    air_radio_set_bind_mode(input->air_config.radio);
    input->state = AIR_INPUT_BIND_STATE_RX;
    input->bind_packet_expires = 0;
    input->send_response_at = TIME_MICROS_MAX;
    input->bind_accepted = false;
    input->bind_confirmation_sent = false;
    input->bind_completed = false;
    input->band_index = 0;
    input->switch_band_at = time_micros_now() + BAND_SWITCH_INTERVAL_US;
    if (!input_air_bind_update_band(input))
    {
        LOG_W(TAG, "No air bands supported");
        return false;
    }
    air_radio_start_rx(input->air_config.radio);
    led_mode_add(LED_MODE_BIND);
    return true;
}

static void input_air_bind_send_response(void *data, time_micros_t now)
{
    input_air_bind_t *input = data;
    air_role_e role = AIR_ROLE_RX_AWAITING_CONFIRMATION;
    if (input->bind_accepted)
    {
        role = AIR_ROLE_RX;
        input->bind_confirmation_sent = true;
    }
    air_bind_packet_t bind_packet = {
        .addr = input->air.addr,
        .key = input->bind_packet.key,
        .role = role,
    };
    bind_packet.info.modes = input->air_config.modes;
    const char *name = rc_data_get_craft_name(input->input.rc_data);
    memset(bind_packet.name, 0, sizeof(bind_packet.name));
    if (name)
    {
        strlcpy(bind_packet.name, name, sizeof(bind_packet.name));
    }
    air_bind_packet_prepare(&bind_packet);
    LOG_I(TAG, "Sending bind response");
    input->state = AIR_INPUT_BIND_STATE_TX;
    air_radio_send(input->air_config.radio, &bind_packet, sizeof(bind_packet));
}

static bool input_air_bind_update(void *data, rc_data_t *rc_data, time_micros_t now)
{
    input_air_bind_t *input = data;
    air_radio_t *radio = input->air_config.radio;
    air_bind_packet_t pkt;
    switch (input->state)
    {
    case AIR_INPUT_BIND_STATE_RX:
        led_mode_set(LED_MODE_BIND_WITH_REQUEST, input->bind_packet_expires > now);
        if (air_radio_is_rx_done(radio))
        {
            size_t n = air_radio_read(radio, &pkt, sizeof(pkt));
            if (n == sizeof(pkt) && air_bind_packet_validate(&pkt) && pkt.role == AIR_ROLE_TX)
            {
                LOG_I(TAG, "Got bind request");
                air_bind_packet_cpy(&input->bind_packet, &pkt);
                input->bind_packet_expires = now + MILLIS_TO_MICROS(AIR_BIND_PACKET_EXPIRATION_MS);
                // Wait 10ms to send a response
                input->send_response_at = now + MILLIS_TO_MICROS(10);
            }
            // Not actually required since now we always send
            // a response, but if we change that in the future
            // we'd need to re-enable RX mode.
            air_radio_sleep(radio);
            air_radio_start_rx(radio);
        }
        else if (now > input->send_response_at)
        {
            // Time to send a response back
            input_air_bind_send_response(input, now);
            input->send_response_at = TIME_MICROS_MAX;
        }
        else if (now > input->switch_band_at)
        {
            if (now > input->bind_packet_expires)
            {
                // Packet has expired, switch bands
                input->band_index++;
                input_air_bind_update_band(input);
                air_radio_start_rx(radio);
            }
            input->switch_band_at = now + BAND_SWITCH_INTERVAL_US;
        }
        break;
    case AIR_INPUT_BIND_STATE_TX:
        if (air_radio_is_tx_done(radio))
        {
            if (input->bind_accepted && input->bind_confirmation_sent)
            {
                // Bind was accepted and the confirmation to the TX
                // was just sent.
                input->bind_completed = true;
            }
            else
            {
                // Finished transmitting an informative packet to the TX,
                // continue in bind mode.
                air_radio_sleep(radio);
                air_radio_start_rx(radio);
                input->state = AIR_INPUT_BIND_STATE_RX;
            }
        }
        break;
    }
    // Never updates rc_data
    return false;
}

static void input_air_bind_close(void *data, void *config)
{
    input_air_bind_t *input = data;
    air_radio_sleep(input->air_config.radio);
    led_mode_remove(LED_MODE_BIND);
    led_mode_remove(LED_MODE_BIND_WITH_REQUEST);
}

static bool input_air_bind_has_request(void *user_data, air_bind_packet_t *packet, air_band_e *band, bool *needs_confirmation)
{
    input_air_bind_t *input = user_data;
    time_micros_t now = time_micros_now();
    if (now < input->bind_packet_expires)
    {
        air_bind_packet_cpy(packet, &input->bind_packet);
        *band = input->air_config.band;
        *needs_confirmation = true;
        return true;
    }
    return false;
}

static bool input_air_bind_accept_request(void *user_data)
{
    input_air_bind_t *input = user_data;
    input->bind_accepted = true;
    return input->bind_completed;
}

void input_air_bind_init(input_air_bind_t *input_air_bind, air_addr_t addr, air_config_t *air_config)
{
    input_air_bind->air_config = *air_config;
    air_io_bind_t bind = {
        .has_request = input_air_bind_has_request,
        .accept_request = input_air_bind_accept_request,
        .user_data = input_air_bind,
    };
    air_io_init(&input_air_bind->air, addr, &bind, NULL);
    input_air_bind->input.vtable = (input_vtable_t){
        .open = input_air_bind_open,
        .update = input_air_bind_update,
        .close = input_air_bind_close,
    };
}
