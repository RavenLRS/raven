#include <hal/log.h>

#include "air/air_radio.h"

#include "ui/led.h"

#include "output_air_bind.h"

static const char *TAG = "Output.Air.Bind";

static bool output_air_bind_open(void *data, void *config)
{
    LOG_I(TAG, "Start bind");
    output_air_bind_t *output = data;
    output->next_bind_offer = 0;
    output->binding_key = air_key_generate();
    output->has_bind_response = false;
    output->bind_packet_expires = 0;
    air_radio_set_bind_mode(output->air_config.radio);
    air_radio_set_frequency(output->air_config.radio, air_band_frequency(output->air_config.band), 0);
    led_mode_add(LED_MODE_BIND);
    return true;
}

static bool output_air_bind_update(void *data, rc_data_t *rc_data, bool update_rc, time_micros_t now)
{
    output_air_bind_t *output = data;
    air_radio_t *radio = output->air_config.radio;
    led_mode_set(LED_MODE_BIND_WITH_REQUEST, output->bind_packet_expires > now);
    if (output->next_bind_offer < now)
    {
        if (!air_radio_is_tx_done(radio))
        {
            LOG_W(TAG, "TX not finished before sending next bind packet");
        }
        output->is_listening = false;

        air_bind_packet_t bind_packet = {
            .addr = output->air.addr,
            .key = output->binding_key,
            .role = AIR_ROLE_TX,
        };
        bind_packet.info.modes = output->air_config.modes;

        const char *name = rc_data_get_pilot_name(output->output.rc_data);
        memset(bind_packet.name, 0, sizeof(bind_packet.name));
        if (name)
        {
            strlcpy(bind_packet.name, name, sizeof(bind_packet.name));
        }
        air_bind_packet_prepare(&bind_packet);
        LOG_I(TAG, "Sending bind packet");
        air_radio_send(radio, &bind_packet, sizeof(bind_packet));
        output->next_bind_offer = now + AIR_BIND_PACKET_INTERVAL_MS * 1000;
    }
    else
    {
        // Check if TX was done
        if (!output->is_listening && air_radio_is_tx_done(radio))
        {
            air_radio_sleep(radio);
            air_radio_start_rx(radio);
            output->is_listening = true;
        }
        else if (output->is_listening && air_radio_is_rx_done(radio))
        {
            air_bind_packet_t *bind_resp = &output->bind_resp;
            if (air_radio_read(radio, bind_resp, sizeof(*bind_resp)) == sizeof(*bind_resp) &&
                air_bind_packet_validate(bind_resp) && bind_resp->key == output->binding_key)
            {
                // Got a response from the RX. It might be informing that the RX
                // is awaiting confirmation from the user or confirming the bind.
                LOG_I(TAG, "Got bind response (accepted: %s)", bind_resp->role == AIR_ROLE_RX ? "Y" : "N");
                output->bind_packet_expires = now + MILLIS_TO_MICROS(AIR_BIND_PACKET_EXPIRATION_MS);
                output->has_bind_response = true;
            }
        }
    }
    return false;
}

static void output_air_bind_close(void *data, void *config)
{
    output_air_bind_t *output = data;
    air_radio_sleep(output->air_config.radio);
    led_mode_remove(LED_MODE_BIND);
    led_mode_remove(LED_MODE_BIND_WITH_REQUEST);
}

static bool output_air_bind_has_request(void *data, air_bind_packet_t *packet, air_band_e *band, bool *needs_confirmation)
{
    output_air_bind_t *output = data;
    if (output->has_bind_response && time_micros_now() < output->bind_packet_expires)
    {
        air_bind_packet_cpy(packet, &output->bind_resp);
        *band = output->air_config.band;
        *needs_confirmation = output->bind_resp.role != AIR_ROLE_RX;
        return true;
    }
    return false;
}

static bool output_air_bind_accept_request(void *data)
{
    // Nothing to do, just signal we're done
    return true;
}

void output_air_bind_init(output_air_bind_t *output_air_bind, air_addr_t addr, air_config_t *air_config)
{
    output_air_bind->air_config = *air_config;
    air_io_bind_t bind = {
        .has_request = output_air_bind_has_request,
        .accept_request = output_air_bind_accept_request,
        .user_data = output_air_bind,
    };
    air_io_init(&output_air_bind->air, addr, &bind, NULL);
    output_air_bind->output.vtable = (output_vtable_t){
        .open = output_air_bind_open,
        .update = output_air_bind_update,
        .close = output_air_bind_close,
    };
}
