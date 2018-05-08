#include <hal/log.h>

#include "air/air.h"
#include "air/air_lora.h"

#include "config/config.h"

#include "io/lora.h"

#include "output_air.h"

#define CHANNEL_TO_AIR_OUTPUT(ch) RC_CHANNEL_ENCODE_TO_BITS(ch, AIR_CHANNEL_BITS)
#define MAX_DOWNLINK_LOST_PACKETS 5

static const char *TAG = "Output.Air";

#ifdef LORA_DEBUG_CYCLE_TIME
static time_micros_t cycle_begin;
static time_micros_t cycle_end;
#endif

#define MODE_SWITCH_TELEMETRY_ID TELEMETRY_ID_RX_SNR
#define MODE_SWITCH_TELEMETRY_TYPE int8_t
#define MODE_SWITCH_TELEMETRY_GET_VALUE(x) telemetry_get_i8(x, MODE_SWITCH_TELEMETRY_ID)
// Switch to faster mode if SNR is above 4dBm
#define MODE_SWITCH_FASTER_VALUE (4 * 4)
// Switch to a longer mode if the SNR is below 1.5dBm
#define MODE_SWITCH_LONGER_VALUE (1.5 * 4)
#define MODE_SWITCH_WAIT_INTERVAL_US MILLIS_TO_MICROS(1000)

static void output_air_lora_callback(lora_t *lora, lora_callback_reason_e reason, void *data)
{
    output_air_t *output_air = data;
    switch (reason)
    {
    case LORA_CALLBACK_REASON_RX_DONE:
        output_air->rx_done = true;
        break;
    case LORA_CALLBACK_REASON_TX_DONE:
        // Making the LoRa modem sleep before switching into RX modes
        // resets the FIFO, while adding a minimal time overhead since
        // we would need to at least put it in idle to update the
        // payload size.
        lora_sleep(output_air->lora.lora);
        // Don't enable RX mode if there's no telemetry in this cycle,
        // just get ready for the next send.
        if (output_air->expecting_downlink_packet)
        {
            lora_set_payload_size(output_air->lora.lora, sizeof(air_rx_packet_t));
            lora_enable_continous_rx(output_air->lora.lora);
        }
        break;
    }
}

static void output_air_update_lora_mode(output_air_t *output_air)
{
    air_lora_set_parameters(output_air->lora.lora, output_air->air_mode);
    air_cmd_switch_mode_ack_reset(&output_air->switch_air_mode);
    output_air->requested_air_mode = 0;
    output_air->start_switch_air_mode_faster_at = 0;
    output_air->start_switch_air_mode_longer_at = 0;
    output_air->full_cycle_time = air_lora_full_cycle_time(output_air->air_mode);
    output_air->uplink_cycle_time = air_lora_uplink_cycle_time(output_air->air_mode);
    failsafe_set_max_interval(&output_air->output.failsafe, air_lora_tx_failsafe_interval(output_air->air_mode));
}

static void output_air_update_frequency(output_air_t *output_air, unsigned freq_index)
{
    if (output_air->freq_index != freq_index)
    {
        output_air->freq_index = freq_index;
        lora_set_frequency(output_air->lora.lora, output_air->freq_table.freqs[freq_index]);
    }
}

static void output_air_start_switch_air_mode(output_air_t *output_air)
{
    LOG_I(TAG, "Preparing switch to mode %d", output_air->requested_air_mode);
    air_cmd_e cmd = air_cmd_switch_mode_from_mode(output_air->requested_air_mode);
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

static void output_air_lora_start(output_air_t *output_air)
{
    output_air_update_lora_mode(output_air);
    lora_set_tx_power(output_air->lora.lora, output_air->tx_power);
    output_air->tx_power = -1;
    lora_set_sync_word(output_air->lora.lora, air_sync_word(output_air->air.pairing.key));
    air_freq_table_init(&output_air->freq_table, output_air->air.pairing.key, air_lora_band_frequency(output_air->lora.band));
    output_air->freq_index = 0xFF;
    output_air_update_frequency(output_air, 0);
    lora_set_callback(output_air->lora.lora, output_air_lora_callback, output_air);
    output_air->consecutive_downlink_lost_packets = 0;
    output_air->expecting_downlink_packet = false;
}

static void output_air_stream_telemetry_decoded(void *user, int telemetry_id, const void *data, size_t size, time_micros_t now)
{
    output_air_t *output_air = user;
    telemetry_t *t = &output_air->output.rc_data->telemetry_downlink[TELEMETRY_DOWNLINK_GET_IDX(telemetry_id)];
    bool changed = telemetry_set_bytes(t, data, size, now);
    switch (telemetry_id)
    {
    case MODE_SWITCH_TELEMETRY_ID:
    {
        if (air_cmd_switch_mode_ack_in_progress(&output_air->switch_air_mode))
        {
            // We're already switching modes
            //
            // TODO: If we're switching up and we should now switch down,
            // cancel the old switch and start the new one.
            break;
        }
        MODE_SWITCH_TELEMETRY_TYPE value = MODE_SWITCH_TELEMETRY_GET_VALUE(t);
        if (value >= MODE_SWITCH_FASTER_VALUE)
        {
            air_lora_mode_e faster = air_lora_mode_faster(output_air->air_mode, output_air->common_air_modes_mask);
            if (air_lora_mode_is_valid(faster))
            {
                if (output_air->start_switch_air_mode_faster_at == 0)
                {
                    output_air->start_switch_air_mode_faster_at = now + MODE_SWITCH_WAIT_INTERVAL_US;
                }
                else if (now > output_air->start_switch_air_mode_faster_at)
                {
                    output_air->requested_air_mode = faster;
                    output_air_start_switch_air_mode(output_air);
                }
            }
        }
        else if (value <= MODE_SWITCH_LONGER_VALUE)
        {
            air_lora_mode_e longer = air_lora_mode_longer(output_air->air_mode, output_air->common_air_modes_mask);
            if (air_lora_mode_is_valid(longer))
            {
                if (output_air->start_switch_air_mode_longer_at == 0)
                {
                    output_air->start_switch_air_mode_longer_at = now + MODE_SWITCH_WAIT_INTERVAL_US;
                }
                else if (now > output_air->start_switch_air_mode_longer_at)
                {
                    output_air->requested_air_mode = longer;
                    output_air_start_switch_air_mode(output_air);
                }
            }
        }
        else
        {
            output_air->start_switch_air_mode_faster_at = 0;
            output_air->start_switch_air_mode_longer_at = 0;
        }
        break;
    }
    case TELEMETRY_ID_CRAFT_NAME:
        if (changed)
        {
            air_addr_t bound_addr;
            if (air_io_get_bound_addr(&output_air->air, &bound_addr))
            {
                config_set_air_name(&bound_addr, data);
            }
        }
        break;
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
            if (ack->mode == output_air->requested_air_mode)
            {
                // Got confirmation from the RX that we're switching modes
                air_cmd_switch_mode_ack_copy(&output_air->switch_air_mode, ack);
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
            output_air->common_air_modes_mask = air_lora_mode_mask_remove(output_air->common_air_modes_mask, *mode);
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
    for (int ii = 4; ii < data->channels_num; ii++)
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
        lora_set_tx_power(output_air->lora.lora, output_air->tx_power);
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
        air_cmd_switch_mode_ack_reset(&output_air->switch_air_mode);
        if (output_air->air_mode != output_air->air_mode_longest)
        {
            output_air->air_mode = output_air->air_mode_longest;
            output_air_update_lora_mode(output_air);
        }
    }

    if (air_cmd_switch_mode_ack_proceed(&output_air->switch_air_mode, output_air->seq))
    {
        output_air->air_mode = output_air->switch_air_mode.mode;
        LOG_I(TAG, "Switch to mode %d for seq %u", output_air->air_mode, output_air->seq);
        output_air_update_lora_mode(output_air);
    }
    output_air_update_frequency(output_air, output_air->seq);
    air_io_on_frame(&output_air->air, now);
    if (output_air->expecting_downlink_packet)
    {
        LOG_D(TAG, "Missing or invalid downlink packet");
        output_air_stop_ack(output_air, data);
    }
    if (air_lora_cycle_is_full(output_air->air_mode, output_air->seq))
    {
        output_air->next_packet = now + output_air->full_cycle_time;
        output_air->expecting_downlink_packet = true;
    }
    else
    {
        output_air->next_packet = now + output_air->uplink_cycle_time;
        output_air->expecting_downlink_packet = false;
    }
    output_air->is_listening = false;
    output_air->rx_done = false;
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
    int p = 0;
    uint8_t c;
    // Check if we have buffered data to send
    while (p < sizeof(pkt.data) && air_stream_pop_output(&output_air->air_stream, &c))
    {
        pkt.data[p++] = c;
    }
    air_tx_packet_prepare(&pkt, output_air->air.pairing.key);
    lora_send(output_air->lora.lora, &pkt, sizeof(pkt));
    //LOG_BUFFER_I("LORAOUT", &pkt, sizeof(pkt));
}

static void output_air_recv_packet(output_air_t *output_air, rc_data_t *data, time_micros_t now)
{
    air_rx_packet_t in_pkt;
    int rssi, snr, lq;

    output_air->rx_done = false;
    if (lora_read(output_air->lora.lora, &in_pkt, sizeof(in_pkt)) == sizeof(in_pkt))
    {
        //LOG_BUFFER_I("LORAIN", &in_pkt, sizeof(in_pkt));
        if (air_rx_packet_validate(&in_pkt, output_air->air.pairing.key))
        {
            air_stream_feed_input(&output_air->air_stream, in_pkt.seq, in_pkt.data, sizeof(in_pkt.data), now);
            rssi = lora_rssi(output_air->lora.lora, &snr, &lq);
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
#ifdef LORA_DEBUG_CYCLE_TIME
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
    if (!air_lora_modes_intersect(&output_air->common_air_modes_mask, output_air->air.pairing_info.modes, output_air->lora.modes))
    {
        LOG_W(TAG, "No common LoRa modules between RX and this TX");
        return false;
    }
    output_air->air_mode_longest = air_lora_mode_longest(output_air->common_air_modes_mask);
    output_air->air_mode = output_air->air_mode_longest;
    LOG_I(TAG, "Open with key %u", output_air->air.pairing.key);
    output_air_config_t *config_air = config;
    output_air->tx_power = config_air->tx_power;
    output_air->seq = 0;
    output_air->is_listening = false;
    output_air->force_stream_feed = false;
    output_air_lora_start(output_air);
    air_stream_init(&output_air->air_stream, NULL,
                    output_air_stream_telemetry_decoded, output_air_stream_cmd_decoded, output);
    msp_air_init(&output_air->msp_air, &output_air->air_stream, output_air_msp_before_feed, output_air);
    OUTPUT_SET_MSP_TRANSPORT(output_air, MSP_TRANSPORT(&output_air->msp_air));
    return true;
}

static bool output_air_update(void *output, rc_data_t *data, time_micros_t now)
{
    output_air_t *output_air = output;

    if (output_air->rx_done)
    {
        output_air_recv_packet(output_air, data, now);
    }
    if (now > output_air->next_packet)
    {
        output_air_send_control_packet(output_air, data, now);
#ifdef LORA_DEBUG_CYCLE_TIME
        printf("CYCLE %llu\n", cycle_end - cycle_begin);
        cycle_begin = now;
#endif
    }
    return true;
}

static void output_air_close(void *output, void *config)
{
    LOG_I(TAG, "Close");
    output_air_t *output_air = output;
    lora_set_callback(output_air->lora.lora, NULL, NULL);
    lora_sleep(output_air->lora.lora);
}

void output_air_init(output_air_t *output, air_addr_t addr, air_lora_config_t *lora, rmp_t *rmp)
{
    output->lora = *lora;
    output->requested_air_mode = 0;
    output->start_switch_air_mode_faster_at = 0;
    output->start_switch_air_mode_longer_at = 0;
    output->output.flags = OUTPUT_FLAG_REMOTE;
    output->output.vtable = (output_vtable_t){
        .open = output_air_open,
        .update = output_air_update,
        .close = output_air_close,
    };
    rmp_air_init(&output->rmp_air, rmp, &addr, &output->air_stream);
    air_io_init(&output->air, addr, NULL, &output->rmp_air);
    output->output.min_update_interval = 0;
}

void output_air_set_tx_power(output_air_t *output, int tx_power)
{
    output->tx_power = tx_power;
}