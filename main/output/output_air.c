#include <hal/log.h>

#include "air/air_radio.h"

#include "config/config.h"

#include "output_air.h"

#define CHANNEL_TO_AIR_OUTPUT(ch) RC_CHANNEL_ENCODE_TO_BITS(ch, AIR_CHANNEL_BITS)
#define MAX_DOWNLINK_LOST_PACKETS 5

static const char *TAG = "Output.Air";

#ifdef AIR_DEBUG_CYCLE_TIME
static time_micros_t cycle_begin;
static time_micros_t cycle_end;
#endif

#define MODE_SWITCH_WAIT_INTERVAL_US MILLIS_TO_MICROS(1000)

typedef enum
{
    OUTPUT_AIR_STATE_IDLE,    // Waiting for TX
    OUTPUT_AIR_STATE_TX,      // Transmitting
    OUTPUT_AIR_STATE_TX_DONE, // Transmission done, still in TX mode
    OUTPUT_AIR_STATE_RX,      // Receiving
    OUTPUT_AIR_STATE_RX_DONE, // Received frame, still not processed
} output_air_state_e;

static void output_air_radio_callback(air_radio_t *radio, air_radio_callback_reason_e reason, void *data)
{
    output_air_t *output_air = data;
    switch (reason)
    {
    case AIR_RADIO_CALLBACK_REASON_RX_DONE:
        output_air->state = OUTPUT_AIR_STATE_RX_DONE;
        break;
    case AIR_RADIO_CALLBACK_REASON_TX_DONE:
        output_air->state = OUTPUT_AIR_STATE_TX_DONE;
        break;
    }
}

static void output_air_invalidate_mode_sw(output_air_t *output_air)
{
    air_cmd_switch_mode_ack_reset(&output_air->air_modes.sw.ack);
    output_air->air_modes.sw.requested = AIR_MODE_INVALID;
    output_air->air_modes.sw.to_faster_scheduled_at = 0;
    output_air->air_modes.sw.to_longer_scheduled_at = 0;
}

static void output_air_update_mode(output_air_t *output_air)
{
    air_radio_t *radio = output_air->air_config.radio;
    air_mode_e air_mode = output_air->air_modes.current;
    air_radio_set_mode(radio, air_mode);
    output_air->air_modes.faster = air_mode_faster(air_mode, output_air->air_modes.common);
    output_air->air_modes.longer = air_mode_longer(air_mode, output_air->air_modes.common);
    output_air->cycle_time = air_radio_cycle_time(radio, air_mode);
    output_air_invalidate_mode_sw(output_air);
    failsafe_set_max_interval(&output_air->output.failsafe, air_radio_tx_failsafe_interval(radio, air_mode));
}

static void output_air_update_frequency(output_air_t *output_air, unsigned freq_index)
{
    if (output_air->freq_index != freq_index)
    {
        output_air->freq_index = freq_index;
        air_radio_set_frequency(output_air->air_config.radio, output_air->air.freq_table.freqs[freq_index], 0);
    }
}

static void output_air_start_switch_air_mode(output_air_t *output_air)
{
    air_mode_e requested = output_air->air_modes.sw.requested;
    LOG_I(TAG, "Preparing switch to mode %d", requested);
    air_cmd_e cmd = air_cmd_switch_mode_from_mode(requested);
    air_stream_feed_output_cmd(&output_air->air_stream, cmd, NULL, 0);
}

static void output_air_reset_ack(output_air_t *output_air, rc_data_t *data)
{
    for (int ii = 0; ii < ARRAY_COUNT(data->channels); ii++)
    {
        data_state_reset_ack(&data->channels[ii].data_state);
    }
    for (int ii = 0; ii < ARRAY_COUNT(data->telemetry_uplink); ii++)
    {
        data_state_reset_ack(&data->telemetry_uplink[ii].data_state);
    }
}

static void output_air_stop_ack(output_air_t *output_air, rc_data_t *data)
{
    for (int ii = 0; ii < ARRAY_COUNT(data->channels); ii++)
    {
        data_state_stop_ack(&data->channels[ii].data_state);
    }
    for (int ii = 0; ii < ARRAY_COUNT(data->telemetry_uplink); ii++)
    {
        data_state_stop_ack(&data->telemetry_uplink[ii].data_state);
    }
}

static void output_air_start(output_air_t *output_air)
{
    air_radio_t *radio = output_air->air_config.radio;
    unsigned long center_freq = air_band_frequency(output_air->air_config.band);
    air_radio_calibrate(radio, center_freq);
    output_air_update_mode(output_air);
    air_radio_set_tx_power(radio, output_air->tx_power);
    output_air->tx_power = -1;
    air_radio_set_sync_word(radio, air_sync_word(output_air->air.pairing.key));
    air_freq_table_init(&output_air->air.freq_table, output_air->air.pairing.key, center_freq);
    output_air->freq_index = 0xFF;
    output_air_update_frequency(output_air, 0);
    air_radio_set_callback(radio, output_air_radio_callback, output_air);
    output_air->consecutive_downlink_lost_packets = 0;
    output_air->expecting_downlink_packet = false;
}

static void output_air_stream_telemetry_decoded(void *user, int telemetry_id, const void *data, size_t size, time_micros_t now)
{
    output_air_t *output_air = user;
    telemetry_t *t = &output_air->output.rc_data->telemetry_downlink[TELEMETRY_DOWNLINK_GET_IDX(telemetry_id)];
    bool changed = telemetry_set_bytes(t, data, size, now);
    if (telemetry_id == TELEMETRY_ID_CRAFT_NAME)
    {
        if (changed)
        {
            air_addr_t bound_addr;
            if (air_io_get_bound_addr(&output_air->air, &bound_addr))
            {
                config_set_air_name(&bound_addr, data);
            }
        }
    }
    if (!air_cmd_switch_mode_ack_in_progress(&output_air->air_modes.sw.ack))
    {
        // Otherwise we're already switching modes
        //
        // TODO: If we're switching up and we should now switch down,
        // cancel the old switch and start the new one.
        if (air_mode_is_valid(output_air->air_modes.longer) &&
            air_radio_should_switch_to_longer_mode(output_air->air_config.radio,
                                                   output_air->air_modes.current,
                                                   output_air->air_modes.longer,
                                                   telemetry_id,
                                                   t))
        {
            output_air->air_modes.sw.to_faster_scheduled_at = 0;
            if (output_air->air_modes.sw.to_longer_scheduled_at == 0)
            {
                output_air->air_modes.sw.to_longer_scheduled_at = now + MODE_SWITCH_WAIT_INTERVAL_US;
            }
            else if (now > output_air->air_modes.sw.to_longer_scheduled_at)
            {
                output_air->air_modes.sw.requested = output_air->air_modes.longer;
                output_air_start_switch_air_mode(output_air);
            }
        }
        else if (air_mode_is_valid(output_air->air_modes.faster) &&
                 air_radio_should_switch_to_faster_mode(output_air->air_config.radio,
                                                        output_air->air_modes.current,
                                                        output_air->air_modes.faster,
                                                        telemetry_id,
                                                        t))
        {
            output_air->air_modes.sw.to_longer_scheduled_at = 0;
            if (output_air->air_modes.sw.to_faster_scheduled_at == 0)
            {
                output_air->air_modes.sw.to_faster_scheduled_at = now + MODE_SWITCH_WAIT_INTERVAL_US;
            }
            else if (now > output_air->air_modes.sw.to_faster_scheduled_at)
            {
                output_air->air_modes.sw.requested = output_air->air_modes.faster;
                output_air_start_switch_air_mode(output_air);
            }
        }
    }
}

static void output_air_stream_cmd_decoded(void *user, air_cmd_e cmd, const void *data, size_t size, time_micros_t now)
{
    output_air_t *output_air = user;
    switch (cmd)
    {
    case AIR_CMD_SWITCH_MODE_ACK:
        if (size == sizeof(air_cmd_switch_mode_ack_t))
        {
            const air_cmd_switch_mode_ack_t *ack = data;
            if (ack->mode == output_air->air_modes.sw.requested)
            {
                // Got confirmation from the RX that we're switching modes
                air_cmd_switch_mode_ack_copy(&output_air->air_modes.sw.ack, ack);
                LOG_I(TAG, "Got confirmation for switch to mode %d at seq %u (current seq %u)",
                      ack->mode, ack->at_tx_seq,
                      output_air->seq);
            }
        }
        break;
    case AIR_CMD_SWITCH_MODE_1:
    case AIR_CMD_SWITCH_MODE_2:
    case AIR_CMD_SWITCH_MODE_3:
    case AIR_CMD_SWITCH_MODE_4:
    case AIR_CMD_SWITCH_MODE_5:
        // Only sent upstream
        break;
    case AIR_CMD_REJECT_MODE:
    {
        // The RX rejected a mode change. Remove it from the the current modes bitmask
        // XXX: Should we update this in permanent storage?
        if (size == 1)
        {
            const uint8_t *mode = data;
            output_air->air_modes.common = air_mode_mask_remove(output_air->air_modes.common, *mode);
        }
        break;
    }
    case AIR_CMD_MSP:
    {
        msp_conn_t *conn = msp_io_get_conn(&output_air->output.msp);
        if (conn)
        {
            msp_air_dispatch(&output_air->msp_air, conn, data, size);
        }
        break;
    }
    case AIR_CMD_RMP:
        rmp_air_decode(&output_air->rmp_air, data, size);
        break;
    }
}

static size_t output_air_feed_stream(output_air_t *output_air, rc_data_t *data, unsigned cur_seq, time_micros_t now, size_t *count)
{
    control_channel_t *dch = NULL;
    unsigned dchn = 0;
    telemetry_t *dt = NULL;
    int dtidx = -1;
    uint32_t max_score = 0;
    // First, try to fill the buffer with dirty data
    for (unsigned ii = 4; ii < data->channels_num; ii++)
    {
        control_channel_t *ch = &data->channels[ii];
        if (data_state_is_ack_received(&ch->data_state))
        {
            continue;
        }
        uint32_t score = data_state_score(&ch->data_state, now);
        if (score > max_score)
        {
            dch = ch;
            dchn = ii;
            max_score = score;
        }
    }
    for (int ii = 0; ii < TELEMETRY_UPLINK_COUNT; ii++)
    {
        telemetry_t *t = &data->telemetry_uplink[ii];
        if (!telemetry_has_value(t))
        {
            continue;
        }
        if (data_state_is_ack_received(&t->data_state))
        {
            continue;
        }
        uint32_t score = data_state_score(&t->data_state, now);
        if (score > max_score)
        {
            dch = NULL;
            dt = t;
            dtidx = ii;
            max_score = score;
        }
    }
    if (dch)
    {
        size_t n = air_stream_feed_output_channel(&output_air->air_stream, dchn, dch->value);
        *count += n;
        data_state_sent(&dch->data_state, AIR_SEQ_TO_SEND_UPLINK(cur_seq, *count), now);
        return n;
    }
    if (dt)
    {
        size_t n = air_stream_feed_output_uplink_telemetry(&output_air->air_stream, dt, TELEMETRY_UPLINK_ID(dtidx));
        *count += n;
        data_state_sent(&dt->data_state, AIR_SEQ_TO_SEND_UPLINK(cur_seq, *count), now);
        return n;
    }
    // No data to send
    return 0;
}

static void output_air_msp_before_feed(msp_air_t *msp_air, size_t size, void *user_data)
{
    // Always feed one uplink channel or telemetry value after writing an MSP
    // request, to prevent uplink starvation when too may MSP requests come
    output_air_t *output_air = user_data;
    output_air->force_stream_feed = true;
}

static void output_air_send_control_packet(output_air_t *output_air, rc_data_t *data, time_micros_t now)
{
    if (output_air->tx_power >= 0)
    {
        air_radio_set_tx_power(output_air->air_config.radio, output_air->tx_power);
        output_air->tx_power = -1;
    }

    if (failsafe_is_active(&output_air->output.failsafe))
    {
        output_air_reset_ack(output_air, data);

        (void)TELEMETRY_SET_I8(data, TELEMETRY_ID_RX_RSSI_ANT1, 0, now);
        (void)TELEMETRY_SET_I8(data, TELEMETRY_ID_RX_RSSI_ANT2, 0, now);
        (void)TELEMETRY_SET_I8(data, TELEMETRY_ID_RX_SNR, 0, now);
        (void)TELEMETRY_SET_I8(data, TELEMETRY_ID_RX_LINK_QUALITY, 0, now);

        // When the RX goes into FS, it switches to longest mode, so
        // eventually both ends will see each other.
        air_cmd_switch_mode_ack_reset(&output_air->air_modes.sw.ack);
        if (output_air->air_modes.current != output_air->air_modes.longest)
        {
            output_air->air_modes.current = output_air->air_modes.longest;
            output_air_update_mode(output_air);
        }
    }

    if (air_cmd_switch_mode_ack_proceed(&output_air->air_modes.sw.ack, output_air->seq))
    {
        output_air->air_modes.current = output_air->air_modes.sw.ack.mode;
        LOG_I(TAG, "Switch to mode %d for seq %u", output_air->air_modes.current, output_air->seq);
        output_air_update_mode(output_air);
    }
    output_air_update_frequency(output_air, output_air->seq);
    air_io_on_frame(&output_air->air, now);
    if (output_air->expecting_downlink_packet)
    {
        LOG_D(TAG, "Missing or invalid downlink packet");
        output_air_stop_ack(output_air, data);
    }
    output_air->next_packet = now + output_air->cycle_time;
    output_air->expecting_downlink_packet = true;
    // If the input is in failsafe mode, connection with the control side was
    // lost (e.g. cable to the radio was broken?), so we stop sending control
    // frames. If the connection is re-established we start sending again so
    // the link will recover from the FS state.
    if (failsafe_is_active(data->failsafe.input))
    {
        return;
    }
    unsigned cur_seq = output_air->seq;
    air_tx_packet_t pkt = {
        .seq = output_air->seq++,
        .ch0 = CHANNEL_TO_AIR_OUTPUT(data->channels[0].value),
        .ch1 = CHANNEL_TO_AIR_OUTPUT(data->channels[1].value),
        .ch2 = CHANNEL_TO_AIR_OUTPUT(data->channels[2].value),
        .ch3 = CHANNEL_TO_AIR_OUTPUT(data->channels[3].value),
        // We might have no data to send. This leaves the data
        // stream ready to accept data.
        .data = {AIR_DATA_START_STOP, AIR_DATA_START_STOP},
    };
    // Check if we need to generate some data for other channels/telemetry
    size_t count = air_stream_output_count(&output_air->air_stream);
    if (output_air->force_stream_feed)
    {
        output_air->force_stream_feed = false;
        output_air_feed_stream(output_air, data, cur_seq, now, &count);
    }
    while (count < sizeof(pkt.data))
    {
        size_t n = output_air_feed_stream(output_air, data, cur_seq, now, &count);
        if (n == 0)
        {
            // No more data to send
            break;
        }
    }
    size_t p = 0;
    uint8_t c;
    // Check if we have buffered data to send
    while (p < sizeof(pkt.data) && air_stream_pop_output(&output_air->air_stream, &c))
    {
        pkt.data[p++] = c;
    }
    air_tx_packet_prepare(&pkt, output_air->air.pairing.key);
    air_radio_send(output_air->air_config.radio, &pkt, sizeof(pkt));
    //LOG_BUFFER_I("RADIO-OUT", &pkt, sizeof(pkt));
}

static void output_air_recv_packet(output_air_t *output_air, rc_data_t *data, time_micros_t now)
{
    air_radio_t *radio = output_air->air_config.radio;
    air_rx_packet_t in_pkt;
    int rssi, snr, lq;

    if (air_radio_read(radio, &in_pkt, sizeof(in_pkt)) == sizeof(in_pkt))
    {
        //LOG_BUFFER_I("RADIO-IN", &in_pkt, sizeof(in_pkt));
        if (air_rx_packet_validate(&in_pkt, output_air->air.pairing.key))
        {
            air_stream_feed_input(&output_air->air_stream, in_pkt.seq, in_pkt.data, sizeof(in_pkt.data), now);
            rssi = air_radio_rssi(radio, &snr, &lq);
            air_io_update_rssi(&output_air->air, rssi, snr, lq, now);
            output_air->consecutive_downlink_lost_packets = 0;
            output_air->expecting_downlink_packet = false;
            output_air_update_frequency(output_air, output_air->seq);
            failsafe_reset_interval(&output_air->output.failsafe, now);
            output_air->last_downlink_packet_at = now;

            // XXX: This only works when ALL cycles have both uplink and downlink stages
            for (int ii = 0; ii < ARRAY_COUNT(data->channels); ii++)
            {
                data_state_update_ack_received(&data->channels[ii].data_state, in_pkt.tx_seq);
            }
            for (int ii = 0; ii < ARRAY_COUNT(data->telemetry_uplink); ii++)
            {
                data_state_update_ack_received(&data->telemetry_uplink[ii].data_state, in_pkt.tx_seq);
            }
        }
        else
        {
            LOG_W(TAG, "Got invalid packet");
        }
#ifdef AIR_DEBUG_CYCLE_TIME
        cycle_end = now;
#endif
    }
}

static bool output_air_open(void *output, void *config)
{
    output_air_t *output_air = output;
    if (!air_io_is_bound(&output_air->air))
    {
        // No pairing
        return false;
    }
    if (!air_modes_intersect(&output_air->air_modes.common, output_air->air.pairing_info.modes, output_air->air_config.modes))
    {
        LOG_W(TAG, "No common air modules between RX and this TX");
        return false;
    }
    output_air->air_modes.longest = air_mode_longest(output_air->air_modes.common);
    if (!air_mode_is_valid(output_air->air_modes.longest))
    {
        LOG_W(TAG, "Could not determine a valid air mode");
        return false;
    }
    output_air->air_modes.current = output_air->air_modes.longest;
    output_air->air_modes.faster = air_mode_faster(output_air->air_modes.current, output_air->air_modes.common);
    output_air->air_modes.longer = air_mode_longer(output_air->air_modes.current, output_air->air_modes.common);
    output_air_invalidate_mode_sw(output_air);
    LOG_I(TAG, "Open with key %u", (unsigned)output_air->air.pairing.key);
    output_air_config_t *config_air = config;
    output_air->tx_power = config_air->tx_power;
    output_air->seq = 0;
    output_air->force_stream_feed = false;
    output_air->next_packet = 0;
    output_air->state = OUTPUT_AIR_STATE_IDLE;
    output_air_start(output_air);
    air_stream_init(&output_air->air_stream, NULL,
                    output_air_stream_telemetry_decoded, output_air_stream_cmd_decoded, output);
    msp_air_init(&output_air->msp_air, &output_air->air_stream, output_air_msp_before_feed, output_air);
    OUTPUT_SET_MSP_TRANSPORT(output_air, MSP_TRANSPORT(&output_air->msp_air));
    return true;
}

static bool output_air_update(void *output, rc_data_t *data, bool update_rc, time_micros_t now)
{
    output_air_t *output_air = output;

    // If the next scheduled packet is now due, we stop everything
    // else and start transmitting.
    if (now > output_air->next_packet)
    {
        output_air->state = OUTPUT_AIR_STATE_TX;
        output_air_send_control_packet(output_air, data, now);
#ifdef AIR_DEBUG_CYCLE_TIME
        printf("CYCLE %llu\n", cycle_end - cycle_begin);
        cycle_begin = now;
#endif
    }

    switch ((output_air_state_e)output_air->state)
    {
    case OUTPUT_AIR_STATE_IDLE:
        break;
    case OUTPUT_AIR_STATE_TX:
        // Nothing to do, transition to OUTPUT_AIR_STATE_TX_DONE
        // is done via callback
        break;
    case OUTPUT_AIR_STATE_TX_DONE:
        // Making the LoRa modem sleep before switching into RX modes
        // resets the FIFO, while adding a minimal time overhead since
        // we would need to at least put it in idle to update the
        // payload size.
        air_radio_sleep(output_air->air_config.radio);
        // Enable RX mode
        air_radio_set_payload_size(output_air->air_config.radio, sizeof(air_rx_packet_t));
        air_radio_start_rx(output_air->air_config.radio);
        output_air->state = OUTPUT_AIR_STATE_RX;
        break;
    case OUTPUT_AIR_STATE_RX:
        // Nothing to do, transition to OUTPUT_AIR_STATE_RX_DONE
        // is done via callback
        break;
    case OUTPUT_AIR_STATE_RX_DONE:
        output_air_recv_packet(output_air, data, now);
        output_air->state = OUTPUT_AIR_STATE_IDLE;
#if defined(OUTPUT_AIR_AS_FAST_AS_POSSIBLE)
        // This is used to test the maximum update frequency
        // of a given air mode.
        output_air->next_packet = now;
#endif
        break;
    }
    // Tell the output layer to not mess with the channel dirty states
    return false;
}

static void output_air_close(void *output, void *config)
{
    LOG_I(TAG, "Close");
    output_air_t *output_air = output;
    air_radio_t *radio = output_air->air_config.radio;
    air_radio_set_callback(radio, NULL, NULL);
    air_radio_sleep(radio);
}

void output_air_init(output_air_t *output, air_addr_t addr, air_config_t *air_config, rmp_t *rmp)
{
    output->air_config = *air_config;
    output->output.flags = OUTPUT_FLAG_REMOTE;
    output->output.vtable = (output_vtable_t){
        .open = output_air_open,
        .update = output_air_update,
        .close = output_air_close,
    };
    rmp_air_init(&output->rmp_air, rmp, &addr, &output->air_stream);
    air_io_init(&output->air, addr, NULL, &output->rmp_air);
}

void output_air_set_tx_power(output_air_t *output, int tx_power)
{
    output->tx_power = tx_power;
}
