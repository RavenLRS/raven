#pragma once

#include "air/air_lora.h"

typedef struct air_addr_s air_addr_t;
typedef struct air_lora_config_s air_lora_config_t;
typedef struct rc_s rc_t;

void rc_get_air_lora_config(rc_t *rc, air_lora_config_t *lora);
void rc_set_peer_air_lora_config(rc_t *rc, air_addr_t *addr, air_lora_band_e band, air_lora_supported_modes_e modes);
