#pragma once

#include "air/air_band.h"
#include "air/air_mode.h"

typedef struct air_addr_s air_addr_t;
typedef struct air_config_s air_config_t;
typedef struct rc_s rc_t;

void rc_get_air_config(rc_t *rc, air_config_t *air_config);
void rc_set_peer_air_config(rc_t *rc, air_addr_t *addr, air_band_e band, air_supported_modes_e modes);
