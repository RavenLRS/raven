#pragma once

#include "air/air_io.h"

#include "output/output.h"

typedef struct output_air_bind_s
{
    output_t output;
    air_io_t air;
    air_lora_config_t lora;
    air_key_t binding_key;
    bool is_listening;
    time_micros_t next_bind_offer;
    air_bind_packet_t bind_resp;
    bool has_bind_response;
    time_micros_t bind_packet_expires;
} output_air_bind_t;

void output_air_bind_init(output_air_bind_t *output_air_bind, air_addr_t addr, air_lora_config_t *lora);