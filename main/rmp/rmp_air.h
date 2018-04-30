#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "air/air.h"

typedef struct air_stream_s air_stream_t;
typedef struct rmp_s rmp_t;
typedef struct rmp_msg_s rmp_msg_t;

typedef struct rmp_air_s
{
    rmp_t *rmp;
    air_stream_t *stream;
    air_addr_t addr;
    air_addr_t bound_addr;
} rmp_air_t;

void rmp_air_init(rmp_air_t *rmp_air, rmp_t *rmp, air_addr_t *addr, air_stream_t *stream);
void rmp_air_set_bound_addr(rmp_air_t *rmp_air, air_addr_t *bound_addr);
bool rmp_air_encode(rmp_air_t *rmp_air, rmp_msg_t *msg);
void rmp_air_decode(rmp_air_t *rmp_air, const void *data, size_t size);