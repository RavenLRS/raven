#pragma once

#include "air/air_cmd.h"
#include "air/air_config.h"
#include "air/air_io.h"
#include "air/air_stream.h"

#include "output/output.h"

#include "rmp/rmp_air.h"

#include "util/time.h"

#include "msp/msp_air.h"

typedef struct output_air_config_s
{
    int tx_power; // dbm
} output_air_config_t;

typedef struct output_air_s
{
    output_t output;
    air_io_t air;
    air_config_t air_config;
    struct
    {
        air_mode_mask_t common; // Mask with common modes between TX and RX
        air_mode_e current;     // Active mode
        air_mode_e faster;      // Next mode faster than active, might be AIR_MODE_INVALID
        air_mode_e longer;      // Next mode longer than active, might be AIR_MODE_INVALID
        air_mode_e longest;     // Longest mode supported by the current pairing
        struct
        {
            air_mode_e requested; // Requested mode while switching
            air_cmd_switch_mode_ack_t ack;
            time_micros_t to_faster_scheduled_at;
            time_micros_t to_longer_scheduled_at;
        } sw; // Mode switching
    } air_modes;
    bool force_stream_feed;
    time_micros_t last_downlink_packet_at;
    time_micros_t cycle_time;
    time_micros_t next_packet;
    int state;
    unsigned seq : AIR_SEQ_BITS;
    unsigned freq_index;
    air_stream_t air_stream;
    bool expecting_downlink_packet;
    unsigned consecutive_downlink_lost_packets;
    int tx_power;

    msp_air_t msp_air;
    rmp_air_t rmp_air;
} output_air_t;

void output_air_init(output_air_t *output, air_addr_t addr, air_config_t *air_config, rmp_t *rmp);

void output_air_set_tx_power(output_air_t *output, int tx_power);