#include <hal/log.h>

#include "air/air_lora.h"

#include "config/config.h"
#include "io/lora.h"

#include "rc/rc_data.h"

#include "input_air.h"

#define AIR_TO_CHANNEL_INPUT(val) RC_CHANNEL_DECODE_FROM_BITS(val, AIR_CHANNEL_BITS)
#define FULL_CYCLE_TIME_WAIT_FACTOR 1.10f // Wait 110% of the cycle time to decide we've lost a packet
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

typedef enum {
    AIR_INPUT_STATE_RX, // Listening
    AIR_INPUT_STATE_TX, // Transmitting
} air_input_state_e;

static void input_air_update_lora_frequency(input_air_t *input_air, unsigned freq_index)
{
    input_air->freq_index = freq_index;
    lora_set_frequency(input_air->lora, input_air->freq_table.freqs[freq_index]);
    lora_enable_continous_rx(input_air->lora);
}

static void input_air_update_lora_mode(input_air_t *input_air)
{
    air_lora_set_parameters(input_air->lora, input_air->air_mode);
    air_cmd_switch_mode_ack_reset(&input_air->switch_air_mode);
    input_air->full_cycle_time = air_lora_full_cycle_time(input_air->air_mode);
    input_air->uplink_cycle_time = air_lora_uplink_cycle_time(input_air->air_mode);
    failsafe_set_max_interval(&input_air->input.failsafe, air_lora_rx_failsafe_interval(input_air->air_mode));
}

static void input_air_lora_start(input_air_t *input_air)
{
    lora_set_sync_word(input_air->lora, air_sync_word(input_air->air.pairing.key));
    air_freq_table_init(&input_air->freq_table, input_air->air.pairing.key, air_lora_band_frequency(input_air->band));
    lora_set_tx_power(input_air->lora, 17);
    input_air_update_lora_mode(input_air);
    lora_sleep(input_air->lora);
    lora_set_payload_size(input_air->lora, sizeof(air_tx_packet_t));
    input_air_update_lora_frequency(input_air, 0);
    lora_enable_continous_rx(input_air->lora);
    input_air->rx_errors = 0;
    input_air->rx_success = 0;
    input_air->air_state = AIR_INPUT_STATE_RX;
    input_air->tx_seq = 0;
    input_air->next_packet_deadline = TIME_MICROS_MAX;
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
        air_lora_mode_e mode = air_lora_mode_from_cmd(cmd);
        // Make sure we support this mode, reject it otherwise
        if (mode < input_air->air_mode_fastest || mode > input_air->air_mode_longest)
        {
            break;
        }
        if (mode != input_air->air_mode && mode != input_air->switch_air_mode.mode)
        {
            unsigned count = MIN(15, 3 * ((AIR_LORA_MODE_LONGEST + 1) - input_air->air_mode));
            LOG_I(TAG, "Got request for switch to mode %d, %u confirmations", mode, count);
            // Use 3 for MODE_5, 6 for MODE_4, ... up to a maximum of 15
            input_air->switch_air_mode.mode = mode;
            input_air->switch_air_mode.at_tx_seq = (input_air->tx_seq + count + input_air->consecutive_lost_packets) % AIR_SEQ_COUNT;
        }
        break;
    }
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
    int p = 0;
    uint8_t c;
    // Check if we have buffered data to send
    while (p < sizeof(out_pkt.data) && air_stream_pop_output(&input_air->air_stream, &c))
    {
        out_pkt.data[p++] = c;
    }
    // XXX: Reset the LoRa modem before sending. Otherwise sometimes we don't
    // get the TX done interrupt.
    lora_sleep(input_air->lora);
    air_rx_packet_prepare(&out_pkt, input_air->air.pairing.key);
    //LOG_BUFFER_I("LORAOUT", &out_pkt, sizeof(out_pkt));
    input_air->air_state = AIR_INPUT_STATE_TX;
    lora_send(input_air->lora, &out_pkt, sizeof(out_pkt));
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
        input_air_update_lora_mode(input_air);
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
        input_air_update_lora_frequency(input_air, freq_at);
        return true;
    }
    return false;
}

static inline bool input_air_receive(input_air_t *input_air, air_tx_packet_t *pkt)
{
    if (lora_is_rx_done(input_air->lora))
    {
        size_t read_size = lora_read(input_air->lora, pkt, sizeof(*pkt));
        //LOG_BUFFER_I("LORAIN", pkt, read_size);
        if (read_size != sizeof(*pkt) || !air_tx_packet_validate(pkt, input_air->air.pairing.key))
        {
            LOG_W(TAG, "Got invalid frame");
            // Reading the FIFO puts the module in IDLE state because we need
            // to set the FIFO ptr. If we got a corrupt frame we need to enable
            // RX mode again.
            lora_enable_continous_rx(input_air->lora);
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
    if (!air_lora_modes_unpack(input_air->air.pairing_info.modes, &input_air->air_mode_fastest, &input_air->air_mode_longest))
    {
        // No info to retrieve the supported modes
        return false;
    }
    LOG_I(TAG, "Open with key %u", input_air->air.pairing.key);
    input_air->air_mode = input_air->air_mode_longest;
    input_air_lora_start(input_air);
    input_air->seq = 0;
    input_air->consecutive_lost_packets = 0;
    input_air->telemetry_fed_index = 0;
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

    switch ((air_input_state_e)input_air->air_state)
    {
    case AIR_INPUT_STATE_RX:
        if (input_air_receive(input_air, &in_pkt))
        {
            input_air->last_packet_at = now;
            bool cycle_is_full = air_lora_cycle_is_full(input_air->air_mode, in_pkt.seq);
            if (cycle_is_full)
            {
                input_air->next_packet_deadline = now + input_air->full_cycle_time * FULL_CYCLE_TIME_WAIT_FACTOR;
            }
            else
            {
                input_air->next_packet_deadline = now + input_air->uplink_cycle_time * FULL_CYCLE_TIME_WAIT_FACTOR;
            }
            input_air->consecutive_lost_packets = 0;
            input_air->rx_success++;
            input_air->tx_seq = in_pkt.seq;

            rssi = lora_rssi(input_air->lora, &snr, &lq);
            air_io_update_rssi(&input_air->air, rssi, snr, lq, now);
            failsafe_reset_interval(&input_air->input.failsafe, now);
            air_io_on_frame(&input_air->air, now);

            if (cycle_is_full)
            {
                input_air_send_response(input_air, data, now);
            }
            else
            {
                lora_sleep(input_air->lora);
                input_air_prepare_next_receive(input_air);
            }
            updated = true;
            // Do this after the response packet has been sent, otherwise the processing
            // could delay the response too much resulting on a lost cycle.
            rc_data_update_channel(data, 0, AIR_TO_CHANNEL_INPUT(in_pkt.ch0), now);
            rc_data_update_channel(data, 1, AIR_TO_CHANNEL_INPUT(in_pkt.ch1), now);
            rc_data_update_channel(data, 2, AIR_TO_CHANNEL_INPUT(in_pkt.ch2), now);
            rc_data_update_channel(data, 3, AIR_TO_CHANNEL_INPUT(in_pkt.ch3), now);

            air_stream_feed_input(&input_air->air_stream, in_pkt.seq, in_pkt.data, sizeof(in_pkt.data), now);
        }
        else if (now > input_air->next_packet_deadline)
        {
            // Packet was lost
            unsigned lost_tx_seq = input_air_next_expected_tx_seq(input_air);
            input_air->rx_errors++;
            input_air->consecutive_lost_packets++;
            if (air_lora_cycle_is_full(input_air->air_mode, lost_tx_seq))
            {
                input_air->next_packet_deadline = now + input_air->full_cycle_time * FULL_CYCLE_TIME_WAIT_FACTOR;
            }
            else
            {
                input_air->next_packet_deadline = now + input_air->uplink_cycle_time * FULL_CYCLE_TIME_WAIT_FACTOR;
            }
            LOG_W(TAG, "invalid or lost frame, %u consecutive, %f%% error rate",
                  input_air->consecutive_lost_packets,
                  (input_air->rx_errors * 100.0) / (input_air->rx_errors + input_air->rx_success));

            // Don't send downlink telemetry for now. Don't sleep nor interrupt the RX here
            // if the frequency doesn't change, since we might be in the middle of receiving
            // a packet. First priority now is recovering the control link.
            if (input_air_prepare_next_receive(input_air))
            {
                lora_sleep(input_air->lora);
                lora_enable_continous_rx(input_air->lora);
            }
        }
        else if (failsafe_is_active(data->failsafe.input))
        {
            air_cmd_switch_mode_ack_reset(&input_air->switch_air_mode);
            if (input_air->air_mode != input_air->air_mode_longest)
            {
                input_air->air_mode = input_air->air_mode_longest;
                input_air_update_lora_mode(input_air);
            }
            air_io_update_reset_rssi(&input_air->air);
        }
        break;
    case AIR_INPUT_STATE_TX:
        if (lora_is_tx_done(input_air->lora))
        {
            lora_set_payload_size(input_air->lora, sizeof(air_tx_packet_t));
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
    lora_sleep(input_air->lora);
}

void input_air_init(input_air_t *input, air_addr_t addr, lora_t *lora, air_lora_band_e band, rmp_t *rmp)
{
    input->lora = lora;
    input->band = band;
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