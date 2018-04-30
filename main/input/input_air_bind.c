#include <hal/log.h>

#include "air/air_lora.h"

#include "io/lora.h"

#include "rc/rc_data.h"

#include "ui/led.h"

#include "input_air_bind.h"

static const char *TAG = "Input.Air.Bind";

enum
{
    AIR_INPUT_BIND_STATE_RX,
    AIR_INPUT_BIND_STATE_TX,
};

static bool input_air_bind_open(void *data, void *config)
{
    LOG_I(TAG, "Open");
    input_air_bind_t *input = data;
    air_lora_set_parameters_bind(input->lora, input->band);
    input->state = AIR_INPUT_BIND_STATE_RX;
    input->bind_packet_expires = 0;
    input->send_response_at = TIME_MICROS_MAX;
    input->bind_accepted = false;
    input->bind_confirmation_sent = false;
    input->bind_completed = false;
    lora_enable_continous_rx(input->lora);
    led_set_blink_mode(LED_ID_1, LED_BLINK_MODE_BIND);
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
    const char *name = rc_data_get_craft_name(input->input.rc_data);
    memset(bind_packet.name, 0, sizeof(bind_packet.name));
    if (name)
    {
        strlcpy(bind_packet.name, name, sizeof(bind_packet.name));
    }
    air_bind_packet_prepare(&bind_packet);
    LOG_I(TAG, "Sending bind response");
    input->state = AIR_INPUT_BIND_STATE_TX;
    lora_send(input->lora, &bind_packet, sizeof(bind_packet));
}

static bool input_air_bind_update(void *data, rc_data_t *rc_data, time_micros_t now)
{
    input_air_bind_t *input = data;
    air_bind_packet_t pkt;
    switch (input->state)
    {
    case AIR_INPUT_BIND_STATE_RX:
        if (lora_is_rx_done(input->lora))
        {
            size_t n = lora_read(input->lora, &pkt, sizeof(pkt));
            if (n == sizeof(pkt) && air_bind_packet_validate(&pkt) && pkt.role == AIR_ROLE_TX)
            {
                LOG_I(TAG, "Got bind request");
                time_micros_t now = time_micros_now();
                air_bind_packet_cpy(&input->bind_packet, &pkt);
                input->bind_packet_expires = now + MILLIS_TO_MICROS(AIR_BIND_PACKET_EXPIRATION_MS);
                // Wait 10ms to send a response
                input->send_response_at = now + MILLIS_TO_MICROS(10);
            }
            // Not actually required since now we always send
            // a response, but if we change that in the future
            // we'd need to re-enable RX mode.
            lora_enable_continous_rx(input->lora);
        }
        else if (now > input->send_response_at)
        {
            // Time to send a response back
            input_air_bind_send_response(input, now);
            input->send_response_at = TIME_MICROS_MAX;
        }
        break;
    case AIR_INPUT_BIND_STATE_TX:
        if (lora_is_tx_done(input->lora))
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
                lora_sleep(input->lora);
                lora_enable_continous_rx(input->lora);
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
    lora_sleep(input->lora);
    led_set_blink_mode(LED_ID_1, LED_BLINK_MODE_NONE);
}

static bool input_air_bind_has_request(void *user_data, air_bind_packet_t *packet, bool *needs_confirmation)
{
    input_air_bind_t *input = user_data;
    time_micros_t now = time_micros_now();
    if (now < input->bind_packet_expires)
    {
        air_bind_packet_cpy(packet, &input->bind_packet);
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

void input_air_bind_init(input_air_bind_t *input_air_bind, air_addr_t addr, lora_t *lora, air_lora_band_e band)
{
    input_air_bind->lora = lora;
    input_air_bind->band = band;
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