#include <hal/log.h>

#include "air/air_mode.h"
#include "air/air_radio.h"

#include "config/config.h"

#include "rc/rc_data.h"

#include "input_air.h"

#define AIR_TO_CHANNEL_INPUT(val) RC_CHANNEL_DECODE_FROM_BITS(val, AIR_CHANNEL_BITS)
#define CYCLE_TIME_WAIT_FACTOR 0.10f // Wait an extra 10% of the cycle time to decide we've lost a packet
// Maximum number of lost packets to continue jumping forward
#define MAX_LOST_PACKETS_JUMPING_FORWARD (AIR_SEQ_COUNT / 2)

// Telemetry values fed to the output before an MSP reply, to avoid filling
// all the stream with big MSP responses.
static telemetry_downlink_id_e telemetry_fed_before_msp[] = {
    TELEMETRY_ID_RX_RSSI_ANT1,
    TELEMETRY_ID_RX_RSSI_ANT2,
    TELEMETRY_ID_RX_LINK_QUALITY,
    TELEMETRY_ID_RX_SNR,
};

static const char *TAG = "Input.Air";

typedef enum
{
    AIR_INPUT_STATE_RX, // Listening
    AIR_INPUT_STATE_TX, // Transmitting
} air_input_state_e;

static void input_air_update_air_frequency(input_air_t *input_air, unsigned freq_index)
{
    input_air->freq_index = freq_index;
    air_radio_t *radio = input_air->air_config.radio;
    air_freq_table_t *freqs = &input_air->air.freq_table;
    air_radio_set_frequency(radio, freqs->freqs[freq_index], freqs->abs_errors[freq_index]);
    air_radio_start_rx(radio);
}

static void input_air_update_air_mode(input_air_t *input_air)
{
    air_radio_t *radio = input_air->air_config.radio;
    air_radio_set_mode(radio, input_air->air_mode);
    air_cmd_switch_mode_ack_reset(&input_air->switch_air_mode);
    input_air->cycle_time = air_radio_cycle_time(radio, input_air->air_mode);
    failsafe_set_max_interval(&input_air->input.failsafe, air_radio_rx_failsafe_interval(radio, input_air->air_mode));
    input_air->reset_rssi = true;
}

static void input_air_start(input_air_t *input_air)
{
    air_radio_t *radio = input_air->air_config.radio;
    unsigned long center_freq = air_band_frequency(input_air->air_config.band);
    air_radio_calibrate(radio, center_freq);
    air_radio_set_sync_word(radio, air_sync_word(input_air->air.pairing.key));
    air_freq_table_init(&input_air->air.freq_table, input_air->air.pairing.key, center_freq);
    // TODO: RX used 17dBm fixed power
    air_radio_set_tx_power(radio, 17);
    input_air_update_air_mode(input_air);
    air_radio_sleep(radio);
    air_radio_set_payload_size(radio, sizeof(air_tx_packet_t));
    input_air_update_air_frequency(input_air, 0);
    input_air->rx_errors = 0;
    input_air->rx_success = 0;
    input_air->air_state = AIR_INPUT_STATE_RX;
    input_air->tx_seq = 0;
    input_air->next_packet_deadline = TIME_MICROS_MAX;
    input_air->next_packet_deadline_extended = false;
}

static void input_air_stream_channel_decoded(void *user, unsigned chn, unsigned value, time_micros_t now)
{
    input_air_t *input_air = user;
    rc_data_update_channel(input_air->input.rc_data, chn, value, now);
}

static void input_air_stream_telemetry_decoded(void *user, int telemetry_id, const void *data, size_t size, time_micros_t now)
{
    input_air_t *input_air = user;
    telemetry_t *t = &input_air->input.rc_data->telemetry_uplink[TELEMETRY_UPLINK_GET_IDX(telemetry_id)];
    bool changed = telemetry_set_bytes(t, data, size, now);
    if (telemetry_id == TELEMETRY_ID_PILOT_NAME && changed)
    {
        air_addr_t bound_addr;
        if (air_io_get_bound_addr(&input_air->air, &bound_addr))
        {
            config_set_air_name(&bound_addr, data);
        }
    }
}

static void input_air_stream_cmd_decoded(void *user, air_cmd_e cmd, const void *data, size_t size, time_micros_t now)
{
    input_air_t *input_air = user;
    switch (cmd)
    {
    case AIR_CMD_SWITCH_MODE_ACK:
        // Only sent downlink
        break;
    case AIR_CMD_SWITCH_MODE_1:
    case AIR_CMD_SWITCH_MODE_2:
    case AIR_CMD_SWITCH_MODE_3:
    case AIR_CMD_SWITCH_MODE_4:
    case AIR_CMD_SWITCH_MODE_5:
    {
        air_mode_e mode = air_mode_from_cmd(cmd);
        // Make sure we support this mode, reject it otherwise
        if (!air_mode_mask_contains(input_air->common_air_modes_mask, mode))
        {
            uint8_t mode8 = mode;
            air_stream_feed_output_cmd(&input_air->air_stream,
                                       AIR_CMD_REJECT_MODE, &mode8, sizeof(mode8));
            break;
        }
        if (mode != input_air->air_mode && mode != input_air->switch_air_mode.mode)
        {
            unsigned count = air_radio_confirmations_required_for_switching_modes(input_air->air_config.radio, input_air->air_mode, mode);
            LOG_I(TAG, "Got request for switch to mode %d, %u confirmations", mode, count);
            input_air->switch_air_mode.mode = mode;
            input_air->switch_air_mode.at_tx_seq = (input_air->tx_seq + count + input_air->consecutive_lost_packets) % AIR_SEQ_COUNT;
        }
        break;
    }
    case AIR_CMD_REJECT_MODE:
        // Nothing to do here, the air input does't request mode changes
        break;
    case AIR_CMD_MSP:
    {
        msp_conn_t *conn = msp_io_get_conn(&input_air->input.msp);
        if (conn)
        {
            msp_air_dispatch(&input_air->msp_air, conn, data, size);
        }
        break;
    }
    case AIR_CMD_RMP:
        rmp_air_decode(&input_air->rmp_air, data, size);
        break;
    }
}

static size_t input_air_feed_stream_ack(input_air_t *input_air)
{
    if (air_cmd_switch_mode_ack_in_progress(&input_air->switch_air_mode))
    {
        // Empty the output buffer so we can guarantee the ACK
        // is gonna fit in the next packet.
        air_stream_reset_output(&input_air->air_stream);
        // This has priority over anything else.
        return air_stream_feed_output_cmd(&input_air->air_stream,
                                          AIR_CMD_SWITCH_MODE_ACK, &input_air->switch_air_mode,
                                          sizeof(input_air->switch_air_mode));
    }
    return 0;
}

static void input_air_msp_before_feed(msp_air_t *msp_air, size_t size, void *user_data)
{
    // Feed some telemetry before sending an MSP response, to update the TX with the
    // RSSI/LQ/SNR status from the RX since it might need the information. If we need
    // to send an ACK for a mode change, it takes priority over the telemetry.
    input_air_t *input_air = user_data;
    telemetry_downlink_id_e id = telemetry_fed_before_msp[input_air->telemetry_fed_index++];
    telemetry_t *telemetry = rc_data_get_downlink_telemetry(input_air->input.rc_data, id);
    air_stream_feed_output_downlink_telemetry(&input_air->air_stream, telemetry, id);
    if (input_air->telemetry_fed_index == ARRAY_COUNT(telemetry_fed_before_msp))
    {
        input_air->telemetry_fed_index = 0;
    }
}

static size_t input_air_feed_stream(input_air_t *input_air, rc_data_t *data, time_micros_t now)
{
    telemetry_t *dt = NULL;
    int dtidx = -1;
    uint32_t max_score = 0;
    for (int ii = 0; ii < TELEMETRY_DOWNLINK_COUNT; ii++)
    {
        telemetry_t *t = &data->telemetry_downlink[ii];
        if (!telemetry_has_value(t))
        {
            continue;
        }
        uint32_t score = data_state_score(&t->data_state, now);
        if (score > max_score)
        {
            dt = t;
            dtidx = ii;
            max_score = score;
        }
    }
    if (dt)
    {
        data_state_sent(&dt->data_state, -1, now);
        return air_stream_feed_output_downlink_telemetry(&input_air->air_stream, dt, TELEMETRY_DOWNLINK_ID(dtidx));
    }
    // No telemetry data to send
    return 0;
}

static void input_air_send_response(input_air_t *input_air, rc_data_t *data, time_micros_t now)
{
    air_rx_packet_t out_pkt = {
        .seq = input_air->seq++,
        .tx_seq = input_air->tx_seq,
        .data = {AIR_DATA_START_STOP, AIR_DATA_START_STOP, AIR_DATA_START_STOP},
    };

    if (input_air_feed_stream_ack(input_air) == 0)
    {
        // Only send non-ACK data if we have no ACK to send
        size_t count = air_stream_output_count(&input_air->air_stream);
        while (count < sizeof(out_pkt.data))
        {
            size_t n = input_air_feed_stream(input_air, data, now);
            if (n == 0)
            {
                break;
            }
            count += n;
        }
    }
    size_t p = 0;
    uint8_t c;
    // Check if we have buffered data to send
    while (p < sizeof(out_pkt.data) && air_stream_pop_output(&input_air->air_stream, &c))
    {
        out_pkt.data[p++] = c;
    }
    // XXX: Reset the LoRa modem before sending. Otherwise sometimes we don't
    // get the TX done interrupt.
    air_radio_sleep(input_air->air_config.radio);
    air_rx_packet_prepare(&out_pkt, input_air->air.pairing.key);
    //LOG_BUFFER_I("RADIO-OUT", &out_pkt, sizeof(out_pkt));
    input_air->air_state = AIR_INPUT_STATE_TX;
    air_radio_send(input_air->air_config.radio, &out_pkt, sizeof(out_pkt));
}

static unsigned input_air_next_expected_tx_seq(input_air_t *input_air)
{
    return (input_air->tx_seq + 1 + input_air->consecutive_lost_packets) % AIR_SEQ_COUNT;
}

// Returns wether a frequency change happened
static bool input_air_prepare_next_receive(input_air_t *input_air)
{
    if (air_cmd_switch_mode_ack_in_progress(&input_air->switch_air_mode) &&
        air_cmd_switch_mode_ack_proceed(&input_air->switch_air_mode, input_air_next_expected_tx_seq(input_air)))
    {
        LOG_I(TAG, "Switch to mode %d for TX seq %u", input_air->switch_air_mode.mode, input_air->switch_air_mode.at_tx_seq);
        // Time to switch modes
        input_air->air_mode = input_air->switch_air_mode.mode;
        input_air_update_air_mode(input_air);
    }

    // Start hopping on reverse if the FS becomes too long. The TX might
    // have been restarted.
    unsigned freq_at;
    if (input_air->consecutive_lost_packets > MAX_LOST_PACKETS_JUMPING_FORWARD)
    {
        // Dwell for 4x as much on each frequency during FS
        unsigned decrease = (input_air->consecutive_lost_packets - MAX_LOST_PACKETS_JUMPING_FORWARD) / 4;
        freq_at = (input_air->tx_seq + MAX_LOST_PACKETS_JUMPING_FORWARD - decrease) % AIR_SEQ_COUNT;
    }
    else
    {
        // This works because we use as many frequencies as seq numbers
        _Static_assert(AIR_NUM_HOPPING_FREQS == AIR_SEQ_COUNT, "AIR_NUM_HOPPING_FREQS != AIR_SEQ_COUNT");
        freq_at = input_air_next_expected_tx_seq(input_air);
    }
    // This is required for clock synchonization. Otherwise we could be resetting the LoRa
    // modem in the middle of the reception of a frame.
    if (freq_at != input_air->freq_index)
    {
        input_air_update_air_frequency(input_air, freq_at);
        return true;
    }
    return false;
}

static inline bool input_air_receive(input_air_t *input_air, air_tx_packet_t *pkt)
{
    air_radio_t *radio = input_air->air_config.radio;
    if (air_radio_is_rx_done(radio))
    {
        size_t read_size = air_radio_read(radio, pkt, sizeof(*pkt));
        //LOG_BUFFER_I("RADIO-IN", pkt, read_size);
        if (read_size != sizeof(*pkt) || !air_tx_packet_validate(pkt, input_air->air.pairing.key))
        {
            LOG_W(TAG, "Got invalid frame");
            // Reading the FIFO puts the module in IDLE state because we need
            // to set the FIFO ptr. If we got a corrupt frame we need to enable
            // RX mode again.
            air_radio_start_rx(radio);
            return false;
        }
        return true;
    }
    return false;
}

static bool input_air_open(void *input, void *config)
{
    input_air_t *input_air = input;
    if (!air_io_is_bound(&input_air->air))
    {
        return false;
    }
    if (!air_modes_intersect(&input_air->common_air_modes_mask, input_air->air.pairing_info.modes, input_air->air_config.modes))
    {
        LOG_W(TAG, "No common air modes between TX and this RX");
        return false;
    }
    LOG_I(TAG, "Open with key %u", input_air->air.pairing.key);
    input_air->air_mode_longest = air_mode_longest(input_air->common_air_modes_mask);
    if (!air_mode_is_valid(input_air->air_mode_longest))
    {
        LOG_W(TAG, "Could not determine a valid air mode");
        return false;
    }
    input_air->air_mode = input_air->air_mode_longest;

    input_air_start(input_air);
    input_air->seq = 0;
    input_air->consecutive_lost_packets = 0;
    input_air->telemetry_fed_index = 0;
    input_air->reset_rssi = true;
    air_stream_init(&input_air->air_stream, input_air_stream_channel_decoded,
                    input_air_stream_telemetry_decoded, input_air_stream_cmd_decoded, input);
    msp_air_init(&input_air->msp_air, &input_air->air_stream, input_air_msp_before_feed, input_air);
    INPUT_SET_MSP_TRANSPORT(input_air, MSP_TRANSPORT(&input_air->msp_air));
    return true;
}

static bool input_air_update(void *input, rc_data_t *data, time_micros_t now)
{
    input_air_t *input_air = input;
    air_tx_packet_t in_pkt;
    int rssi, snr, lq;
    bool updated = false;
    air_radio_t *radio = input_air->air_config.radio;

    switch ((air_input_state_e)input_air->air_state)
    {
    case AIR_INPUT_STATE_RX:
        if (failsafe_is_active(data->failsafe.input))
        {
            air_cmd_switch_mode_ack_reset(&input_air->switch_air_mode);
            if (input_air->air_mode != input_air->air_mode_longest)
            {
                input_air->air_mode = input_air->air_mode_longest;
                input_air_update_air_mode(input_air);
            }
            air_io_invalidate_rssi(&input_air->air, now);
        }

        if (input_air_receive(input_air, &in_pkt))
        {
            input_air->last_packet_at = now;
            input_air->next_packet_expected_at = now + input_air->cycle_time;
            input_air->next_packet_deadline = input_air->next_packet_expected_at + input_air->cycle_time * CYCLE_TIME_WAIT_FACTOR;
            input_air->next_packet_deadline_extended = false;
            input_air->consecutive_lost_packets = 0;
            input_air->rx_success++;
            input_air->tx_seq = in_pkt.seq;

            rssi = air_radio_rssi(radio, &snr, &lq);
            int last_error = air_radio_frequency_error(radio);
            input_air->air.freq_table.abs_errors[input_air->tx_seq] += last_error;
            input_air->air.freq_table.last_errors[input_air->tx_seq] = last_error;

            input_air_send_response(input_air, data, now);

            // Do this after the response packet has been sent, otherwise the processing
            // could delay the response too much resulting on a lost cycle.
            if (input_air->reset_rssi)
            {
                air_io_reset_rssi(&input_air->air, rssi, snr, lq, now);
                input_air->reset_rssi = false;
            }
            else
            {
                air_io_update_rssi(&input_air->air, rssi, snr, lq, now);
            }
            failsafe_reset_interval(&input_air->input.failsafe, now);
            air_io_on_frame(&input_air->air, now);
            updated = true;
            rc_data_update_channel(data, 0, AIR_TO_CHANNEL_INPUT(in_pkt.ch0), now);
            rc_data_update_channel(data, 1, AIR_TO_CHANNEL_INPUT(in_pkt.ch1), now);
            rc_data_update_channel(data, 2, AIR_TO_CHANNEL_INPUT(in_pkt.ch2), now);
            rc_data_update_channel(data, 3, AIR_TO_CHANNEL_INPUT(in_pkt.ch3), now);

            air_stream_feed_input(&input_air->air_stream, in_pkt.seq, in_pkt.data, sizeof(in_pkt.data), now);
            break;
        }
        if (now > input_air->next_packet_deadline)
        {
            if (!input_air->next_packet_deadline_extended && air_radio_is_rx_in_progress(radio))
            {
                input_air->next_packet_deadline += input_air->cycle_time * CYCLE_TIME_WAIT_FACTOR;
                input_air->next_packet_deadline_extended = true;
                break;
            }
            // Packet was lost
            input_air->rx_errors++;
            input_air->consecutive_lost_packets++;
            input_air->next_packet_expected_at = now + input_air->cycle_time;
            input_air->next_packet_deadline = input_air->next_packet_expected_at + input_air->cycle_time * CYCLE_TIME_WAIT_FACTOR;
            input_air->next_packet_deadline_extended = false;
            LOG_W(TAG, "invalid or lost frame, %u consecutive, %f%% error rate",
                  input_air->consecutive_lost_packets,
                  (input_air->rx_errors * 100.0) / (input_air->rx_errors + input_air->rx_success));

            // Don't send downlink telemetry for now. Don't sleep nor interrupt the RX here
            // if the frequency doesn't change, since we might be in the middle of receiving
            // a packet. First priority now is recovering the control link.
            if (input_air_prepare_next_receive(input_air))
            {
                air_radio_sleep(radio);
                air_radio_start_rx(radio);
            }
            break;
        }
        break;
    case AIR_INPUT_STATE_TX:
        if (air_radio_is_tx_done(radio))
        {
            air_radio_set_payload_size(radio, sizeof(air_tx_packet_t));
            input_air_prepare_next_receive(input_air);
            input_air->air_state = AIR_INPUT_STATE_RX;
        }
        break;
    }
    return updated;
}

static void input_air_close(void *input, void *config)
{
    LOG_I(TAG, "Close");
    input_air_t *input_air = input;
    air_radio_sleep(input_air->air_config.radio);
}

void input_air_init(input_air_t *input, air_addr_t addr, air_config_t *air_config, rmp_t *rmp)
{
    input->air_config = *air_config;
    input->input.vtable = (input_vtable_t){
        .open = input_air_open,
        .update = input_air_update,
        .close = input_air_close,
    };
    // Note that the air_stream is not initialized yet, but we won't
    // send anything until it's bound.
    rmp_air_init(&input->rmp_air, rmp, &addr, &input->air_stream);
    air_io_init(&input->air, addr, NULL, &input->rmp_air);
}
