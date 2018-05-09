#pragma once

#include <stdbool.h>

#include "air/air.h"
#include "air/air_lora.h"

#include "util/lpf.h"
#include "util/time.h"

typedef bool (*air_io_has_request_f)(void *user_data, air_bind_packet_t *packet, air_lora_band_e *band, bool *needs_confirmation);
typedef bool (*air_io_accept_request_f)(void *user_data);

typedef struct air_io_bind_s
{
    air_io_has_request_f has_request;
    air_io_accept_request_f accept_request;
    void *user_data;
} air_io_bind_t;

typedef struct rmp_air_s rmp_air_t;

typedef struct air_io_s
{
    air_addr_t addr;
    air_pairing_t pairing;
    air_info_t pairing_info;
    air_io_bind_t bind;
    rmp_air_t *rmp;
    lpf_t rssi;
    lpf_t snr;
    lpf_t lq;
    lpf_t average_frame_interval;
    time_micros_t last_frame_received;
} air_io_t;

void air_io_init(air_io_t *io, air_addr_t addr, air_io_bind_t *bind, rmp_air_t *rmp);

bool air_io_has_bind_request(air_io_t *io, air_bind_packet_t *packet, air_lora_band_e *band, bool *needs_confirmation);
bool air_io_accept_bind_request(air_io_t *io);
void air_io_bind(air_io_t *io, air_pairing_t *pairing);
bool air_io_is_bound(air_io_t *io);
bool air_io_get_bound_addr(air_io_t *io, air_addr_t *addr);
void air_io_on_frame(air_io_t *io, time_micros_t now);
void air_io_update_rssi(air_io_t *io, int rssi, int snr, int lq, time_micros_t now);
void air_io_update_reset_rssi(air_io_t *io);
unsigned air_io_get_update_frequency(const air_io_t *io);