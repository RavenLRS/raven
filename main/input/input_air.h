#pragma once

#include "air/air_cmd.h"
#include "air/air_freq.h"
#include "air/air_io.h"
#include "air/air_lora.h"
#include "air/air_stream.h"

#include "input/input.h"

#include "msp/msp_air.h"

#include "rmp/rmp_air.h"

#include "util/time.h"

typedef struct input_air_s
{
    input_t input;
    air_io_t air;
    air_lora_config_t lora;
    air_lora_mode_mask_t common_air_modes_mask;
    air_lora_mode_e air_mode_longest;
    int rx_errors;
    int rx_success;
    unsigned seq : AIR_SEQ_BITS;
    unsigned tx_seq : AIR_SEQ_BITS;
    air_stream_t air_stream;
    air_lora_mode_e air_mode;
    air_cmd_switch_mode_ack_t switch_air_mode;
    unsigned air_state;
    unsigned consecutive_lost_packets;
    unsigned telemetry_fed_index;
    time_micros_t full_cycle_time;
    time_micros_t uplink_cycle_time;
    time_micros_t last_packet_at;
    time_micros_t next_packet_deadline;
    air_freq_table_t freq_table;
    unsigned freq_index;

    msp_air_t msp_air;
    rmp_air_t rmp_air;
} input_air_t;

void input_air_init(input_air_t *input, air_addr_t addr, air_lora_config_t *lora, rmp_t *rmp);