
#include <stdint.h>
#include <string.h>

#include <hal/log.h>

#include "air/air.h"
#include "air/air_rf_power.h"

#include "config/config.h"

#include "io/pwm.h"

#include "platform/dispatch.h"
#include "platform/system.h"

#include "rc/rc-private.h"
#include "rc/rc_data.h"

#include "rmp/rmp.h"
#include "rmp/rmp_air.h"

#include "util/crc.h"
#include "util/lpf.h"
#include "util/macros.h"

#include "rc.h"

static const char *TAG = "RC";

#define GET_AIR_IO_FILTERED_FIELD(rc, field) ({ \
    air_io_t *__air_io = rc_get_air_io(rc);     \
    __air_io ? lpf_value(&__air_io->field) : 0; \
})

typedef struct rc_rmp_msp_s
{
    uint16_t cmd;
    uint16_t payload_size;
    uint8_t payload[512];
} PACKED rc_rmp_msp_t;

static void rc_update_tx_pilot_name(rc_t *rc, time_micros_t now)
{
    char buf[AIR_MAX_NAME_LENGTH + 1];
    air_addr_t addr;
    const char *name = settings_get_key_string(SETTING_KEY_TX_PILOT_NAME);
    if (!name || !name[0])
    {
        addr = config_get_addr();
        air_addr_format(&addr, buf, sizeof(buf));
        name = buf;
    }
    (void)TELEMETRY_SET_STR(&rc->data, TELEMETRY_ID_PILOT_NAME, name, now);
}

static void rc_update_rx_craft_name(rc_t *rc, time_micros_t now)
{
    char buf[AIR_MAX_NAME_LENGTH + 1];
    air_addr_t addr;
    const char *name = settings_get_key_string(SETTING_KEY_RX_CRAFT_NAME);
    if (!name || !name[0])
    {
        addr = config_get_addr();
        air_addr_format(&addr, buf, sizeof(buf));
        name = buf;
    }
    (void)TELEMETRY_SET_STR(&rc->data, TELEMETRY_ID_CRAFT_NAME, name, now);
}

// Initialize local telemetry values
static void rc_data_initialize(rc_t *rc)
{
    char buf[AIR_MAX_NAME_LENGTH + 1];
    air_addr_t addr;
    air_info_t air_info;
    time_micros_t now = time_micros_now();
    unsigned channels_num = RC_CHANNELS_NUM;
    switch (rc_get_mode(rc))
    {
    case RC_MODE_TX:
        rc_update_tx_pilot_name(rc, now);
        if (air_io_get_bound_addr(&rc->outputs.air.air, &addr))
        {
            if (!config_get_air_name(buf, sizeof(buf), &addr))
            {
                air_addr_format(&addr, buf, sizeof(buf));
            }
            (void)TELEMETRY_SET_STR(&rc->data, TELEMETRY_ID_CRAFT_NAME, buf, now);

            if (config_get_air_info(&air_info, NULL, &addr))
            {
                channels_num = air_info.channels;
            }
        }
        rmp_set_role(rc->rmp, AIR_ROLE_TX);
        rmp_set_name(rc->rmp, telemetry_get_str(rc_data_get_uplink_telemetry(&rc->data, TELEMETRY_ID_PILOT_NAME), TELEMETRY_ID_PILOT_NAME));
        break;
    case RC_MODE_RX:
        rc_update_rx_craft_name(rc, now);
        if (air_io_get_bound_addr(&rc->inputs.air.air, &addr))
        {
            if (!config_get_air_name(buf, sizeof(buf), &addr))
            {
                air_addr_format(&addr, buf, sizeof(buf));
            }
            (void)TELEMETRY_SET_STR(&rc->data, TELEMETRY_ID_PILOT_NAME, buf, now);

            if (config_get_air_info(&air_info, NULL, &addr))
            {
                channels_num = air_info.channels;
            }
        }
        rmp_set_role(rc->rmp, AIR_ROLE_RX);
        rmp_set_name(rc->rmp, telemetry_get_str(rc_data_get_downlink_telemetry(&rc->data, TELEMETRY_ID_CRAFT_NAME), TELEMETRY_ID_CRAFT_NAME));
        break;
    }
    if (channels_num > 0)
    {
        rc->data.channels_num = MIN(channels_num, RC_CHANNELS_NUM);
    }
    rc->data.ready = false;

    // TX
    (void)TELEMETRY_SET_I8(&rc->data, TELEMETRY_ID_TX_RF_POWER, 20, time_micros_now());

    // RX
    (void)TELEMETRY_SET_U8(&rc->data, TELEMETRY_ID_RX_ACTIVE_ANT, 1, time_micros_now());
    (void)TELEMETRY_SET_I8(&rc->data, TELEMETRY_ID_RX_RF_POWER, 20, time_micros_now());
}

static void rc_invalidate_air(rc_t *rc)
{
    switch (rc_get_mode(rc))
    {
    case RC_MODE_TX:
        rc_invalidate_output(rc);
        break;
    case RC_MODE_RX:
        rc_invalidate_input(rc);
        break;
    default:
        UNREACHABLE();
    }
}

static int rc_get_tx_rf_power(rc_t *rc)
{
    return air_rf_power_to_dbm(settings_get_key_u8(SETTING_KEY_TX_RF_POWER));
}

static bool rc_should_enable_power_test(rc_t *rc)
{
    return settings_get_key_u8(SETTING_KEY_RF_POWER_TEST);
}

static air_io_t *rc_get_air_io(rc_t *rc)
{
    switch (rc_get_mode(rc))
    {
    case RC_MODE_TX:
        if (rc->state.bind_active)
        {
            return &rc->outputs.air_bind.air;
        }
        return &rc->outputs.air.air;
    case RC_MODE_RX:
        if (rc->state.bind_active)
        {
            return &rc->inputs.air_bind.air;
        }
        return &rc->inputs.air.air;
    }
    UNREACHABLE();
    return NULL;
}

static bool rc_get_pair_addr(rc_t *rc, air_addr_t *addr)
{
    air_io_t *air_io = rc_get_air_io(rc);
    return air_io && air_io_get_bound_addr(air_io, addr);
}

static air_band_e rc_get_air_band(rc_t *rc)
{
    switch (rc_get_mode(rc))
    {
    case RC_MODE_TX:
    {
        config_air_band_e config_band = settings_get_key_u8(SETTING_KEY_AIR_BAND);
        return config_get_air_band(config_band);
    }
    case RC_MODE_RX:
    {
        air_pairing_t pairing;
        if (config_get_paired_tx(&pairing))
        {
            air_band_e band;
            if (config_get_air_info(NULL, &band, &pairing.addr))
            {
                return band;
            }
        }
        break;
    }
    }
    return AIR_BAND_INVALID;
}

static void rc_invalidate_pair_air_config(rc_t *rc)
{
    rc->state.pair_air_config_next_req = time_ticks_now();
}

static bool rc_needs_pair_air_config(rc_t *rc)
{
    return rc->state.pair_air_config_next_req > 0;
}

static void rc_update_pair_air_config(rc_t *rc)
{
    time_ticks_t now = time_ticks_now();
    if (now >= rc->state.pair_air_config_next_req)
    {
        air_addr_t pair_addr;
        if (rc_get_pair_addr(rc, &pair_addr))
        {
            rc_rmp_request_air_config(&rc->state.rc_rmp, &pair_addr);
        }
        rc->state.pair_air_config_next_req = now + MILLIS_TO_TICKS(500);
    }
}

static void rc_pair_air_config_updated(rc_t *rc)
{
    rc->state.pair_air_config_next_req = 0;
}

void rc_get_air_config(rc_t *rc, air_config_t *air_config)
{
    air_supported_modes_e supported_modes = 0;
    switch (rc_get_mode(rc))
    {
    case RC_MODE_TX:
        // TX Supports all modes for now
        supported_modes = AIR_SUPPORTED_MODES_1_TO_5;
        break;
    case RC_MODE_RX:
    {
        // RX allows configurable modes
        config_air_mode_e config_modes = settings_get_key_u8(SETTING_KEY_RX_SUPPORTED_MODES);
        supported_modes = config_get_air_modes(config_modes);
        break;
    }
    }
    air_config->radio = rc->radio;
    air_config->band = rc_get_air_band(rc);
    air_config->modes = supported_modes;
    air_config->bands = config_get_air_band_mask();
}

void rc_set_peer_air_config(rc_t *rc, air_addr_t *addr, air_band_e band, air_supported_modes_e modes)
{
    // TODO: Use the band here for better multiband module support
    air_info_t info;
    if (!config_get_air_info(&info, NULL, addr))
    {
        // No config for this peer, nothing to do
        return;
    }
    info.modes = modes;
    bool changed = config_set_air_info(addr, &info, band);
    air_addr_t pair_addr;
    if (rc_get_pair_addr(rc, &pair_addr) && air_addr_equals(addr, &pair_addr))
    {
        rc_pair_air_config_updated(rc);
        if (changed)
        {
            rc_invalidate_air(rc);
        }
    }
}

static void rc_reconfigure_input(rc_t *rc)
{
    LOG_I(TAG, "Reconfigure input");
    union {
        input_crsf_config_t crsf;
    } input_config;
    if (rc->input != NULL)
    {
        input_close(rc->input, rc->input_config);
        rc->input = NULL;
        rc->data.failsafe.input = NULL;
        rc->input_config = NULL;
    }
    memset(&rc->inputs, 0, sizeof(rc->inputs));
    switch (rc_get_mode(rc))
    {
    case RC_MODE_TX:
        switch (config_get_input_type())
        {
        case TX_INPUT_CRSF:
            input_crsf_init(&rc->inputs.crsf);
            rc->input = (input_t *)&rc->inputs.crsf;
            input_config.crsf.gpio = settings_get_key_gpio(SETTING_KEY_TX_TX_GPIO);
            rc->input_config = &input_config.crsf;
            break;
        case TX_INPUT_FAKE:
            input_fake_init(&rc->inputs.fake);
            rc->input = (input_t *)&rc->inputs.fake;
            rc->input_config = NULL;
            break;
        }
        break;
    case RC_MODE_RX:
    {
        if (rc_should_enable_power_test(rc))
        {
            // Use no input
            break;
        }

#if defined(CONFIG_RAVEN_FAKE_INPUT)
        input_fake_init(&rc->inputs.fake);
        rc->inputs.fake.update_interval = FREQ_TO_MICROS(100);
        rc->input = (input_t *)&rc->inputs.fake;
        rc->input_config = NULL;
#else
        air_pairing_t pairing;
        air_config_t air_config;

        rmp_set_pairing(rc->rmp, NULL);
        rc_get_air_config(rc, &air_config);
        if (rc->state.bind_active)
        {
            input_air_bind_init(&rc->inputs.air_bind, config_get_addr(), &air_config);
            rc->input = (input_t *)&rc->inputs.air_bind;
        }
        else
        {
            input_air_init(&rc->inputs.air, config_get_addr(), &air_config, rc->rmp);
            rc->input = (input_t *)&rc->inputs.air;
            if (config_get_paired_tx(&pairing))
            {
                air_io_bind(&rc->inputs.air.air, &pairing);
                rmp_set_pairing(rc->rmp, &pairing);
            }
        }

        rc_invalidate_pair_air_config(rc);
#endif
        break;
    }
    }

    if (rc->input != NULL)
    {
        rc->data.failsafe.input = &rc->input->failsafe;
        input_open(&rc->data, rc->input, rc->input_config);
        msp_conn_t *input_msp = msp_io_get_conn(&rc->input->msp);
        if (input_msp)
        {
            rc_connect_msp_input(rc, input_msp);
        }
    }
    rc_data_initialize(rc);
}

static void rc_reconfigure_output(rc_t *rc)
{
    union {
        output_air_config_t air;
        output_crsf_config_t crsf;
        output_fport_config_t fport;
        output_msp_config_t msp;
        output_sbus_config_t sbus;
    } output_config;

    LOG_I(TAG, "Reconfigure output");

    if (rc->output != NULL)
    {
        output_close(rc->output, rc->output_config);
        rc->output = NULL;
        rc->data.failsafe.output = NULL;
        rc->output_config = NULL;
    }
    memset(&rc->outputs, 0, sizeof(rc->outputs));
    air_pairing_t pairing;
    air_config_t air_config;
    switch (rc_get_mode(rc))
    {
    case RC_MODE_TX:
    {
        rmp_set_pairing(rc->rmp, NULL);
        rc_get_air_config(rc, &air_config);

        if (rc_should_enable_power_test(rc))
        {
            output_air_rf_power_test_init(&rc->outputs.air_power_test, &air_config);
            rc->output = (output_t *)&rc->outputs.air_power_test;
            break;
        }

        if (rc->state.bind_active)
        {
            output_air_bind_init(&rc->outputs.air_bind, config_get_addr(), &air_config);
            rc->output = (output_t *)&rc->outputs.air_bind;
            break;
        }

        output_air_init(&rc->outputs.air, config_get_addr(), &air_config, rc->rmp);
        rc->output = (output_t *)&rc->outputs.air;
        output_config.air.tx_power = rc_get_tx_rf_power(rc);
        if (config_get_paired_rx(&pairing, NULL))
        {
            air_io_bind(&rc->outputs.air.air, &pairing);
            rmp_set_pairing(rc->rmp, &pairing);
        }
        rc->output_config = &output_config.air;

        rc_invalidate_pair_air_config(rc);
        break;
    }
    case RC_MODE_RX:
        if (rc_should_enable_power_test(rc))
        {
            rc_get_air_config(rc, &air_config);
            output_air_rf_power_test_init(&rc->outputs.air_power_test, &air_config);
            rc->output = (output_t *)&rc->outputs.air_power_test;
            break;
        }

        switch (config_get_output_type())
        {
        case RX_OUTPUT_MSP:
            output_msp_init(&rc->outputs.msp);
            rc->output = (output_t *)&rc->outputs.msp;
            output_config.msp.tx = settings_get_key_gpio(SETTING_KEY_RX_TX_GPIO);
            output_config.msp.rx = settings_get_key_gpio(SETTING_KEY_RX_RX_GPIO);
            output_config.msp.baud_rate = settings_get_key_u8(SETTING_KEY_RX_MSP_BAUDRATE);
            rc->output_config = &output_config.msp;
            break;
        case RX_OUTPUT_CRSF:
            output_crsf_init(&rc->outputs.crsf);
            rc->output = (output_t *)&rc->outputs.crsf;
            output_config.crsf.tx = settings_get_key_gpio(SETTING_KEY_RX_TX_GPIO);
            output_config.crsf.rx = settings_get_key_gpio(SETTING_KEY_RX_RX_GPIO);
            output_config.crsf.inverted = false;
            rc->output_config = &output_config.crsf;
            break;
        case RX_OUTPUT_FPORT:
            output_fport_init(&rc->outputs.fport);
            rc->output = (output_t *)&rc->outputs.fport;
            output_config.fport.tx = settings_get_key_gpio(SETTING_KEY_RX_TX_GPIO);
            output_config.fport.rx = settings_get_key_gpio(SETTING_KEY_RX_RX_GPIO);
            output_config.fport.inverted = settings_get_key_bool(SETTING_KEY_RX_FPORT_INVERTED);
            rc->output_config = &output_config.fport;
            break;
        case RX_OUTPUT_SBUS_SPORT:
            output_sbus_init(&rc->outputs.sbus);
            rc->output = (output_t *)&rc->outputs.sbus;
            output_config.sbus.sbus = settings_get_key_gpio(SETTING_KEY_RX_TX_GPIO);
            output_config.sbus.sbus_inverted = settings_get_key_bool(SETTING_KEY_RX_SBUS_INVERTED);
            output_config.sbus.sport = settings_get_key_gpio(SETTING_KEY_RX_RX_GPIO);
            output_config.sbus.sport_inverted = settings_get_key_bool(SETTING_KEY_RX_SPORT_INVERTED);
            rc->output_config = &output_config.sbus;
            break;
        case RX_OUTPUT_NONE:
            output_none_init(&rc->outputs.none);
            rc->output = (output_t *)&rc->outputs.none;
            break;
        }

#if defined(CONFIG_RAVEN_USE_PWM_OUTPUTS)
        // Update PWM output configuration
        pwm_update_config();
#endif

        break;
    }

    if (rc->output != NULL)
    {
        rc->data.failsafe.output = &rc->output->failsafe;
        output_open(&rc->data, rc->output, rc->output_config);
    }
    rc_data_initialize(rc);
}

static void rc_update_binding(rc_t *rc)
{
    if (!rc->state.bind_active)
    {
        return;
    }

    air_io_t *air_io = rc_get_air_io(rc);
    if (!air_io)
    {
        return;
    }
    air_bind_packet_t packet;
    air_band_e band;
    air_pairing_t pairing;
    bool needs_confirmation = true;
    if (air_io_has_bind_request(air_io, &packet, &band, &needs_confirmation) &&
        (rc->state.accept_bind || !needs_confirmation))
    {
        LOG_I(TAG, "Bind accepted");
        if (!air_io_accept_bind_request(air_io))
        {
            // Bind request is being accepted (e.g. the RX might send)
            // a confirmation to the TX. Wait until it's done.
            return;
        }
        LOG_I(TAG, "Bind done");
        air_bind_packet_get_pairing(&packet, &pairing);
        air_io_bind(air_io, &pairing);
        switch (rc_get_mode(rc))
        {
        case RC_MODE_TX:
            config_add_paired_rx(&pairing);
            break;
        case RC_MODE_RX:
            config_set_paired_tx(&pairing);
            break;
        }
        // Store peer information
        config_set_air_info(&pairing.addr, &packet.info, band);

        // Ignore the current pairing alternatives, to avoid
        // popping a dialog just after pairing a new RX while
        // another one is still powered.
        rc_dismiss_alternative_pairings(rc);
        // Bind request accepted, disable bind mode
        const setting_t *bind_setting = settings_get_key(SETTING_KEY_BIND);
        setting_set_bool(bind_setting, false);
    }
}

static void rc_rssi_update(rc_t *rc)
{
    air_io_t *air_io = rc_get_air_io(rc);
    if (!air_io)
    {
        return;
    }
    // TODO: Should we make it 16 bits or apply some offset
    // to represent lower values?
    int rssi = CONSTRAIN_TO_I8(lpf_value(&air_io->rssi));
    float snr = CONSTRAIN_TO_I8(lpf_value(&air_io->snr));
    int8_t lq = lpf_value(&air_io->lq);
    time_micros_t now = time_micros_now();
    switch (rc_get_mode(rc))
    {
    case RC_MODE_TX:
        (void)TELEMETRY_SET_I8(&rc->data, TELEMETRY_ID_TX_RSSI_ANT1, rssi, now);
        (void)TELEMETRY_SET_I8(&rc->data, TELEMETRY_ID_TX_SNR, snr, now);
        (void)TELEMETRY_SET_I8(&rc->data, TELEMETRY_ID_TX_LINK_QUALITY, lq, now);
        break;
    case RC_MODE_RX:
        (void)TELEMETRY_SET_I8(&rc->data, TELEMETRY_ID_RX_RSSI_ANT1, rssi, now);
        (void)TELEMETRY_SET_I8(&rc->data, TELEMETRY_ID_RX_RSSI_ANT2, rssi, now);
        (void)TELEMETRY_SET_I8(&rc->data, TELEMETRY_ID_RX_SNR, snr, now);
        (void)TELEMETRY_SET_I8(&rc->data, TELEMETRY_ID_RX_LINK_QUALITY, lq, now);
        break;
    }
}

bool rc_send_rmp(rmp_t *rmp, rmp_msg_t *msg, void *user_data)
{
    rc_t *rc = user_data;
    air_io_t *air_io = rc_get_air_io(rc);
    if (air_io)
    {
        rmp_air_t *rmp_air = air_io->rmp;
        if (rmp_air)
        {
            return rmp_air_encode(rmp_air, msg);
        }
    }
    return false;
}

static bool rc_rmp_msp_validate(rmp_req_t *req)
{
    if (req->is_authenticated)
    {
        const rc_rmp_msp_t *msp_resp = req->msg->payload;
        // Check that the size matches
        size_t rmp_payload_size = sizeof(*msp_resp) - sizeof(msp_resp->payload) + msp_resp->payload_size;
        return rmp_payload_size == req->msg->payload_size;
    }
    return false;
}

static rc_rmp_resp_ctx_t *rc_rmp_alloc_resp_ctx(rc_t *rc)
{
    // Try to find an empty one
    time_ticks_t now = time_ticks_now();
    for (int ii = 0; ii < ARRAY_COUNT(rc->state.msp_resp_ctx); ii++)
    {
        if (rc->state.msp_resp_ctx[ii].allocated_at == 0)
        {
            rc->state.msp_resp_ctx[ii].allocated_at = now;
            return &rc->state.msp_resp_ctx[ii];
        }
    }
    // Try to find an expired one. If we don't have more space, we
    // look for requests more than 3s old.
    time_ticks_t delta = SECS_TO_TICKS(3);
    if (now >= delta)
    {
        time_ticks_t threshold = now - delta;
        for (int ii = 0; ii < ARRAY_COUNT(rc->state.msp_resp_ctx); ii++)
        {
            if (rc->state.msp_resp_ctx[ii].allocated_at < threshold)
            {
                rc->state.msp_resp_ctx[ii].allocated_at = now;
                return &rc->state.msp_resp_ctx[ii];
            }
        }
    }
    return NULL;
}

static void rc_rmp_free_resp_ctx(rc_rmp_resp_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
}

// Handle the MSP response from an MSP request originated via RMP
static void rc_rmp_msp_request_response_handler(msp_conn_t *conn, uint16_t cmd, const void *payload, int size, void *callback_data)
{
    // Note that this callback will run on core 1, while RMP runs
    // on core 0, so we need some synchronization. TODO: Locking
    rc_rmp_resp_ctx_t *ctx = callback_data;
    rc_rmp_msp_t resp = {
        .cmd = cmd,
        .payload_size = size,
    };
    size_t cpy_size = MIN(size, sizeof(resp.payload));
    memcpy(resp.payload, payload, cpy_size);
    size_t rmp_payload_size = sizeof(resp) - sizeof(resp.payload) + cpy_size;
    rmp_send(ctx->rc->rmp, ctx->rc->state.msp_recv_port, &ctx->dst, ctx->dst_port, &resp, rmp_payload_size);
    rc_rmp_free_resp_ctx(ctx);
}

// Handle an MSP request from RMP and forward it to the output's MSP
static void rc_rmp_msp_request_handler(rmp_t *rmp, rmp_req_t *req, void *user_data)
{
    // Note that typically this will be run on core 0, while the RC
    // pipeline runs on core 1. TODO: Add locking
    rc_t *rc = user_data;

    // Got an MSP request from the TX via RMP
    if (rc_get_mode(rc) != RC_MODE_RX || !rc_rmp_msp_validate(req))
    {
        return;
    }
    msp_conn_t *request_output = msp_io_get_conn(&rc->output->msp);
    if (request_output)
    {
        const rc_rmp_msp_t *msp_req = req->msg->payload;
        rc_rmp_resp_ctx_t *ctx = rc_rmp_alloc_resp_ctx(rc);
        if (!ctx)
        {
            LOG_W(TAG, "Could not allocate rc_rmp_ctx_t, ignoring request");
            return;
        }
        ctx->rc = rc;
        air_addr_cpy(&ctx->dst, &req->msg->src);
        ctx->dst_port = req->msg->src_port;
        msp_conn_send(request_output, msp_req->cmd, msp_req->payload, msp_req->payload_size,
                      rc_rmp_msp_request_response_handler, ctx);
    }
}

static void rc_rmp_msp_response_handler(rmp_t *rmp, rmp_req_t *req, void *user_data)
{
    // Got an MSP response from RX via RMP
    // TODO: Make sure we're in TX mode
    if (!rc_rmp_msp_validate(req))
    {
        return;
    }

    rc_rmp_msp_port_t *rmp_msp_port = user_data;
    msp_conn_t *reply_output = rmp_msp_port->conn;
    if (reply_output)
    {
        const rc_rmp_msp_t *msp_resp = req->msg->payload;
        msp_conn_write(reply_output, MSP_DIRECTION_FROM_MWC, msp_resp->cmd, msp_resp->payload, msp_resp->payload_size);
    }
}

// Returns the internal port assigned to this msp_conn_t or NULL
// if no port can be allocated.
static const rmp_port_t *rc_rmp_msp_port(rc_t *rc, msp_conn_t *conn)
{
    for (int ii = 0; ii < ARRAY_COUNT(rc->state.rmp_msp_port); ii++)
    {
        if (rc->state.rmp_msp_port[ii].conn == conn)
        {
            return rc->state.rmp_msp_port[ii].port;
        }
    }
    for (int ii = 0; ii < ARRAY_COUNT(rc->state.rmp_msp_port); ii++)
    {
        if (!rc->state.rmp_msp_port[ii].conn)
        {
            const rmp_port_t *port = rmp_open_port(rc->rmp, 0, rc_rmp_msp_response_handler, &rc->state.rmp_msp_port[ii]);
            if (port)
            {
                rc->state.rmp_msp_port[ii].conn = conn;
                rc->state.rmp_msp_port[ii].port = port;
            }
            return port;
        }
    }
    LOG_W(TAG, "Not interning MSP connection, no space left");
    return NULL;
}

static void rc_msp_response_callback(msp_conn_t *conn, uint16_t cmd, const void *payload, int size, void *callback_data)
{
    // Callback with response from MWC to send back to the MSP which produced the request
    // (received as callback_data). Note that we don't need to handle RMP here since this
    // function is only called when we forward the request via MSP-to-MSP.
    // See rc_rmp_msp_request_handler() and rc_rmp_msp_response_handler().
    msp_conn_t *reply_output = callback_data;
    if (reply_output)
    {
        msp_conn_write(reply_output, MSP_DIRECTION_FROM_MWC, cmd, payload, size);
    }
}

static void rc_msp_request_callback(msp_conn_t *conn, uint16_t cmd, const void *payload, int size, void *callback_data)
{
    // Request coming from input's MSP has been decoded. Sent it to the output's MSP.
    rc_t *rc = callback_data;
    if (rc_get_mode(rc) == RC_MODE_TX)
    {
        // If we're a TX, try to send it via RMP
        air_io_t *air_io = rc_get_air_io(rc);
        air_addr_t pair_addr;
        if (air_io && air_io_get_bound_addr(air_io, &pair_addr) && rmp_has_p2p_peer(rc->rmp, &pair_addr))
        {
            const rmp_port_t *port = rc_rmp_msp_port(rc, conn);
            if (port)
            {
                rc_rmp_msp_t req = {
                    .cmd = cmd,
                    .payload_size = size,
                };
                size_t cpy_size = MIN(size, sizeof(req.payload));
                memcpy(req.payload, payload, cpy_size);
                size_t rmp_payload_size = sizeof(req) - sizeof(req.payload) + cpy_size;
                if (rmp_send(rc->rmp, port, &pair_addr, RMP_PORT_MSP, &req, rmp_payload_size))
                {
                    return;
                }
            }
        }
    }
    msp_conn_t *request_output = msp_io_get_conn(&rc->output->msp);
    if (request_output)
    {
        // Send the conn argument as the callback data so the response is routed back
        // to the sender.
        msp_conn_send(request_output, cmd, payload, size, rc_msp_response_callback, conn);
    }
}

static void rc_send_air_config_to_pair(rc_t *rc)
{
    // XXX: modes/band config was changed so we need to inform the other
    // end. For now, assume they're available via P2P.
    air_addr_t pair_addr;
    if (rc_get_pair_addr(rc, &pair_addr))
    {
        rc_rmp_send_air_config(&rc->state.rc_rmp, &pair_addr, true);
    }
}

static void rc_setting_changed(const setting_t *setting, void *user_data)
{
    rc_t *rc = user_data;

    if (STR_EQUAL(setting->key, SETTING_KEY_RC_MODE))
    {
        // Reboot after 500ms to allow the setting to be written
        dispatch_after((dispatch_fn)system_reboot, NULL, 500);
    }
    else if (STR_EQUAL(setting->key, SETTING_KEY_BIND))
    {
        rc->state.bind_requested = setting_get_bool(setting);
    }
    else if (SETTING_IS(setting, SETTING_KEY_AIR_BAND))
    {
        rc_send_air_config_to_pair(rc);
        rc_invalidate_air(rc);
    }
    else if (SETTING_IS(setting, SETTING_KEY_RF_POWER_TEST))
    {
        if (rc_get_mode(rc) == RC_MODE_RX)
        {
            rc_invalidate_input(rc);
        }
        rc_invalidate_output(rc);
    }
    else
    {
        switch (rc_get_mode(rc))
        {
        case RC_MODE_TX:
            if (STR_EQUAL(setting->key, SETTING_KEY_TX_RF_POWER))
            {
                // This notification could arrive on any thread, so
                // we can't just change the power from here, schedule
                // it and change it in the main RC loop.
                rc->state.tx_rf_power = rc_get_tx_rf_power(rc);
                break;
            }
            if (STR_HAS_PREFIX(setting->key, SETTING_KEY_RECEIVERS_RX_SELECT_PREFIX))
            {
                // Switch receivers
                int rx_num = setting_receiver_get_rx_num(setting);
                air_pairing_t pairing;
                if (config_get_paired_rx_at(&pairing, rx_num))
                {
                    rc_switch_pairing(rc, &pairing);
                }
                break;
            }
            if (STR_HAS_PREFIX(setting->key, SETTING_KEY_RECEIVERS_RX_DELETE_PREFIX))
            {
                // Receiver has been deleted. Check if it's the current one and invalidate
                // the output.
                int rx_num = setting_receiver_get_rx_num(setting);
                air_pairing_t pairing;
                air_addr_t bound_addr;
                bool is_current_pairing = false;
                if (config_get_paired_rx_at(&pairing, rx_num))
                {
                    air_io_t *io = rc_get_air_io(rc);
                    if (io && air_io_get_bound_addr(io, &bound_addr))
                    {
                        is_current_pairing = air_addr_equals(&pairing.addr, &bound_addr);
                    }
                }
                config_remove_paired_rx_at(rx_num);
                if (is_current_pairing)
                {
                    rc_switch_pairing(rc, NULL);
                }
                break;
            }
            if (STR_HAS_PREFIX(setting->key, SETTING_KEY_TX_PREFIX) &&
                !STR_EQUAL(setting->key, SETTING_KEY_TX_PILOT_NAME))
            {
                rc_invalidate_input(rc);
                break;
            }

            if (SETTING_IS(setting, SETTING_KEY_TX_PILOT_NAME))
            {
                rc_update_tx_pilot_name(rc, time_micros_now());
            }
            break;
        case RC_MODE_RX:
            if (STR_HAS_PREFIX(setting->key, SETTING_KEY_RX_PREFIX) &&
                !STR_EQUAL(setting->key, SETTING_KEY_RX_AUTO_CRAFT_NAME) &&
                !STR_EQUAL(setting->key, SETTING_KEY_RX_CRAFT_NAME))

            {
                rc_invalidate_output(rc);
            }
            if (SETTING_IS(setting, SETTING_KEY_RX_SUPPORTED_MODES))
            {
                rc_send_air_config_to_pair(rc);
                rc_invalidate_input(rc);
            }
#if defined(CONFIG_RAVEN_USE_PWM_OUTPUTS)
            if (STR_HAS_PREFIX(setting->key, SETTING_KEY_RX_CHANNEL_OUTPUTS_PREFIX))
            {
                pwm_update_config();
            }
#endif
            if (SETTING_IS(setting, SETTING_KEY_RX_CRAFT_NAME))
            {
                rc_update_rx_craft_name(rc, time_micros_now());
            }
            break;
        }
    }
}

static bool rc_should_autostart_bind(rc_t *rc)
{
    air_pairing_t pairing;
    switch (rc_get_mode(rc))
    {
    case RC_MODE_TX:
        return !config_get_paired_rx(&pairing, NULL);
    case RC_MODE_RX:
        return !config_get_paired_tx(&pairing);
    }
    return false;
}

static uint8_t rc_crc_addr(uint8_t crc, air_addr_t *addr)
{
    for (int ii = 0; ii < AIR_ADDR_LENGTH; ii++)
    {
        crc = crc_xor(crc, addr->addr[ii]);
    }
    return crc;
}

static inline bool rc_should_update_output(rc_t *rc)
{
    // For now, we let each output decide what to do.
    return true;
}

void rc_init(rc_t *rc, air_radio_t *radio, rmp_t *rmp)
{
    memset(rc, 0, sizeof(*rc));
#if defined(USE_TX_SUPPORT) && defined(USE_RX_SUPPORT)
    rc->state.rc_mode = config_get_rc_mode();
#endif

    rc->radio = radio;
    rc->rmp = rmp;
    rc->input = NULL;
    rc->input_config = NULL;
    rc->output = NULL;
    rc->output_config = NULL;

    rc->data.rmp = rmp;

    rc_rmp_init(&rc->state.rc_rmp, rc, rmp);

    // Invalidate both to make the first iteration
    // open both input and output
    rc->state.invalidate_input = true;
    rc->state.invalidate_output = true;

    rc->state.bind_requested = false;
    rc->state.bind_active = false;
    rc->state.accept_bind = false;

    rc->state.dirty = false;
    rc->state.dismissed_count = 0;
    rc->state.dismissed_pairings = -1;
    rc->state.tx_rf_power = -1;

    settings_add_listener(rc_setting_changed, rc);
    rc->state.msp_recv_port = rmp_open_port(rmp, RMP_PORT_MSP, rc_rmp_msp_request_handler, rc);
    rmp_set_transport(rmp, RMP_TRANSPORT_RC, rc_send_rmp, rc);

#if defined(CONFIG_RAVEN_USE_PWM_OUTPUTS)
    pwm_init();
#endif

    if (rc_should_autostart_bind(rc))
    {
        const setting_t *bind_setting = settings_get_key(SETTING_KEY_BIND);
        setting_set_bool(bind_setting, true);
    }
}

rc_mode_e rc_get_mode(const rc_t *rc)
{
// TODO: This prevents the linker from doing dead code
// elimination when not using both TX and RX support,
// wasting ~30K of flash.
#if defined(USE_TX_SUPPORT) && defined(USE_RX_SUPPORT)
    return rc->state.rc_mode;
#else
    return config_get_rc_mode();
#endif
}

bool rc_is_binding(const rc_t *rc)
{
    return rc->state.bind_active;
}

void rc_accept_bind(rc_t *rc)
{
    rc->state.accept_bind = true;
}

bool rc_has_pending_bind_request(rc_t *rc, air_bind_packet_t *packet)
{
    if (!rc->state.invalidate_input && !rc->state.invalidate_output && rc->state.bind_active)
    {
        air_io_t *air_io = rc_get_air_io(rc);
        if (air_io)
        {
            bool needs_confirmation = false;
            return air_io_has_bind_request(air_io, packet, NULL, &needs_confirmation) && needs_confirmation;
        }
    }
    return false;
}

static bool rc_is_input_failsafe_active(const rc_t *rc, failsafe_reason_e *reason)
{
    if (rc->state.invalidate_input)
    {
        return false;
    }
    const failsafe_t *fs = rc->data.failsafe.input;
    if (!fs)
    {
        return false;
    }
    if (failsafe_is_active(fs))
    {
        if (reason)
        {
            switch (rc_get_mode(rc))
            {
            case RC_MODE_TX:
                // Radio not responding
                *reason = FAILSAFE_REASON_RADIO_LOST;
                break;
            case RC_MODE_RX:
                // Can't see TX on air
                *reason = FAILSAFE_REASON_TX_LOST;
                break;
            }
        }
        return true;
    }
    return false;
}

static bool rc_is_output_failsafe_active(const rc_t *rc, failsafe_reason_e *reason)
{
    if (rc->state.invalidate_output)
    {
        return false;
    }
    const failsafe_t *fs = rc->data.failsafe.output;
    if (!fs)
    {
        return false;
    }
    if (failsafe_is_active(fs))
    {
        if (reason)
        {
            switch (rc_get_mode(rc))
            {
            case RC_MODE_TX:
                // Can't see RX telemetry on air
                *reason = FAILSAFE_REASON_RX_LOST;
                break;
            case RC_MODE_RX:
                // FC was disconnected
                *reason = FAILSAFE_REASON_FC_LOST;
                break;
            }
        }
        return true;
    }
    return false;
}

bool rc_is_failsafe_active(const rc_t *rc, failsafe_reason_e *reason)
{
    return rc_is_input_failsafe_active(rc, reason) || rc_is_output_failsafe_active(rc, reason);
}

int rc_get_rssi_db(rc_t *rc)
{
    switch (rc_get_mode(rc))
    {
    case RC_MODE_TX:
        // Return the RX RSSI
        return TELEMETRY_GET_I8(&rc->data, TELEMETRY_ID_RX_RSSI_ANT1);
    case RC_MODE_RX:
        // Return our own RSSI
        return GET_AIR_IO_FILTERED_FIELD(rc, rssi);
    }
    assert(0 && "unreachable");
    return 0;
}

float rc_get_snr(rc_t *rc)
{
    switch (rc_get_mode(rc))
    {
    case RC_MODE_TX:
        // Return the RX SNR
        return TELEMETRY_GET_I8(&rc->data, TELEMETRY_ID_RX_SNR) / TELEMETRY_SNR_MULTIPLIER;
    case RC_MODE_RX:
        // Return our own SNR
        return GET_AIR_IO_FILTERED_FIELD(rc, snr) / TELEMETRY_SNR_MULTIPLIER;
    }
    assert(0 && "unreachable");
    return 0;
}

int rc_get_rssi_percentage(rc_t *rc)
{
    switch (rc_get_mode(rc))
    {
    case RC_MODE_TX:
        // Return the RX LQ
        return TELEMETRY_GET_I8(&rc->data, TELEMETRY_ID_RX_LINK_QUALITY);
    case RC_MODE_RX:
        // Return our own LQ
        return GET_AIR_IO_FILTERED_FIELD(rc, lq);
    }
    assert(0 && "unreachable");
    return 0;
}

unsigned rc_get_update_frequency(rc_t *rc)
{
    air_io_t *io = rc_get_air_io(rc);
    return io ? air_io_get_update_frequency(io) : 0;
}

bool rc_get_frequencies_table(rc_t *rc, air_freq_table_t *freqs)
{
    air_io_t *io = rc_get_air_io(rc);
    if (io)
    {
        if (freqs)
        {
            memcpy(freqs, &io->freq_table, sizeof(*freqs));
        }
        return true;
    }
    return false;
}

const char *rc_get_pilot_name(rc_t *rc)
{
    return rc_data_get_pilot_name(&rc->data);
}

const char *rc_get_craft_name(rc_t *rc)
{
    return rc_data_get_craft_name(&rc->data);
}

void rc_connect_msp_input(rc_t *rc, msp_conn_t *msp)
{
    msp_conn_set_global_callback(msp, rc_msp_request_callback, rc);
}

int rc_get_alternative_pairings(rc_t *rc, air_pairing_t *pairings, size_t size)
{
    // Don't return any alternatives while a bind is in progress
    if (rc_is_binding(rc) || rc_should_enable_power_test(rc))
    {
        return 0;
    }

    int count = 0;
    uint8_t crc = 0;
    air_addr_t paired_addr = *AIR_ADDR_INVALID;
    air_io_t *air_io = rc_get_air_io(rc);
    if (air_io)
    {
        air_io_get_bound_addr(air_io, &paired_addr);
    }
    // The index for the currently paired TX/RX, < 0 if none
    int paired_idx = -1;
    for (int ii = 0; ii < RMP_MAX_PEERS; ii++)
    {
        // Use an intermediate storage, so we can get the actual count for
        // matching it with rc_dismiss_alternative_pairings().
        air_pairing_t pairing;
        rmp_peer_t *peer = &rc->rmp->internal.peers[ii];
        if (air_addr_is_valid(&peer->addr) && config_get_pairing(&pairing, &peer->addr))
        {
            bool is_valid = false;
            switch (rc_get_mode(rc))
            {
            case RC_MODE_TX:
                is_valid = peer->role == AIR_ROLE_RX;
                break;
            case RC_MODE_RX:
                is_valid = peer->role == AIR_ROLE_TX;
                break;
            }
            if (!is_valid)
            {
                continue;
            }
            crc = rc_crc_addr(crc, &peer->addr);
            if (count < size)
            {
                air_pairing_cpy(&pairings[count], &pairing);
                if (paired_idx < 0 && air_addr_equals(&paired_addr, &peer->addr))
                {
                    paired_idx = ii;
                }
            }
            count++;
        }
    }
    // Check if the user dismissed this pairing
    if (rc->state.dismissed_count == count && rc->state.dismissed_pairings == crc)
    {
        return 0;
    }
    // Not dismissed. Clear the dismissed values, so if an RX re-appears we
    // ask the user again.
    rc->state.dismissed_count = 0;
    rc->state.dismissed_pairings = -1;
    if (paired_idx >= 0)
    {
        if (count == 1)
        {
            // Check if we're just returning the already paired TX/RX, in
            // that case there are no alternatives to switch. Note that
            // we can have cases where just one alternative is fine
            // e.g. the paired RX was switched off and an alternative
            // is live.
            count = 0;
        }
        else
        {
            // Make sure the already paired one is at the top
            if (paired_idx != 0)
            {
                air_pairing_t cpy;
                air_pairing_cpy(&cpy, &pairings[0]);
                air_pairing_cpy(&pairings[0], &pairings[paired_idx]);
                air_pairing_cpy(&pairings[paired_idx], &cpy);
            }
        }
    }
    return MIN(count, size);
}

void rc_switch_pairing(rc_t *rc, air_pairing_t *pairing)
{
    rc_dismiss_alternative_pairings(rc);

    switch (rc_get_mode(rc))
    {
    case RC_MODE_TX:
        if (pairing)
        {
            config_add_paired_rx(pairing);
        }
        rc_invalidate_output(rc);
        break;
    case RC_MODE_RX:
        if (pairing)
        {
            config_set_paired_tx(pairing);
        }
        rc_invalidate_input(rc);
        break;
    }
}

void rc_dismiss_alternative_pairings(rc_t *rc)
{
    uint8_t crc = 0;
    int count = 0;
    for (int ii = 0; ii < RMP_MAX_PEERS; ii++)
    {
        rmp_peer_t *peer = &rc->rmp->internal.peers[ii];
        air_pairing_t pairing;
        if (air_addr_is_valid(&peer->addr) && config_get_pairing(&pairing, &peer->addr))
        {
            count++;
            crc = rc_crc_addr(crc, &peer->addr);
        }
    }
    rc->state.dismissed_count = count;
    rc->state.dismissed_pairings = crc;
}

void rc_invalidate_input(rc_t *rc)
{
    LOG_I(TAG, "Input invalidated");
    rc->state.invalidate_input = true;
}

void rc_invalidate_output(rc_t *rc)
{
    rc->state.invalidate_output = true;
}

void rc_update(rc_t *rc)
{
    if (UNLIKELY(rc->state.invalidate_input))
    {
        rc_reconfigure_input(rc);
        rc->state.invalidate_input = false;
    }
    if (UNLIKELY(rc->state.invalidate_output))
    {
        rc_reconfigure_output(rc);
        rc->state.invalidate_output = false;
    }
    if (UNLIKELY(rc->state.bind_requested != rc->state.bind_active))
    {
        rc->state.accept_bind = false;
        if (rc->state.bind_requested)
        {
            LOG_I(TAG, "Starting bind");
        }
        else
        {
            LOG_I(TAG, "Stopping bind");
        }
        // Invalidate the input or output supporting binding
        // so the previously or new (if any) bound endpoint
        // is configured when stopping bind without binding
        // with a new device.
        switch (rc_get_mode(rc))
        {
        case RC_MODE_TX:
            rc_invalidate_output(rc);
            break;
        case RC_MODE_RX:
            rc_invalidate_input(rc);
            break;
        }
        rc->state.bind_active = rc->state.bind_requested;
    }

    if (UNLIKELY(rc->state.tx_rf_power >= 0))
    {
        output_air_set_tx_power(&rc->outputs.air, rc->state.tx_rf_power);
        rc->state.tx_rf_power = -1;
    }

    time_micros_t now = time_micros_now();
    bool input_new_data = input_update(rc->input, now);
    rc->state.dirty |= input_new_data;
    // We always need to update the output because the air output
    // might need to read the telemetry response before the
    // input is dirty again. Eventually we should refactor this to
    // only call the output if the input is dirty or the output
    // needs to process another data.
    if (LIKELY(rc_should_update_output(rc)))
    {
        rc->state.dirty &= !output_update(rc->output, input_new_data, now);
    }

    if (input_new_data)
    {
#if defined(CONFIG_RAVEN_USE_PWM_OUTPUTS)
        pwm_update(&rc->data);
#endif
    }

    if (UNLIKELY(rc->state.bind_active))
    {
        rc_update_binding(rc);
    }

    if (UNLIKELY(rc_needs_pair_air_config(rc)))
    {
        rc_update_pair_air_config(rc);
    }

    rc_rssi_update(rc);
}
