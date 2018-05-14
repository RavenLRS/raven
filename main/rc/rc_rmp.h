#pragma once

#include <stdbool.h>

#include "air/air_lora.h"

#include "util/macros.h"

typedef struct air_addr_s air_addr_t;
typedef struct rc_s rc_t;
typedef struct rmp_s rmp_t;
typedef struct rmp_port_s rmp_port_t;

typedef enum
{
    RC_RMP_AIR_LORA_CONFIG_REQ = 0,
    RC_RMP_AIR_LORA_CONFIG_RESP,
    RC_RMP_AIR_LORA_CONFIG_ACK,
} rc_rmp_code_e;

typedef struct rc_rmp_air_lora_config_s
{
    uint8_t band;  // Band in use, from air_lora_band_e
    uint8_t modes; // Supported modes, from air_lora_supported_modes_e
    uint8_t ack;   // Wether the sender was an ack. Ignored for RC_RMP_AIR_LORA_CONFIG_ACK.
} PACKED rc_rmp_air_lora_config_t;

typedef struct rc_rmp_msg_s
{
    uint8_t code; // from rc_rmp_code_e
    union {
        rc_rmp_air_lora_config_t lora;
    };
} PACKED rc_rmp_msg_t;

typedef struct rc_rmp_s
{
    rc_t *rc;
    rmp_t *rmp;
    const rmp_port_t *port;
} rc_rmp_t;

void rc_rmp_init(rc_rmp_t *rc_rmp, rc_t *rc, rmp_t *rmp);
void rc_rmp_request_air_lora_config(rc_rmp_t *rc_rmp, air_addr_t *addr);
void rc_rmp_send_air_lora_config(rc_rmp_t *rc_rmp, air_addr_t *addr, bool ack);