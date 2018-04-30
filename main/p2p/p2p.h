#pragma once

#include <stdbool.h>

#include <hal/p2p.h>

typedef struct rmp_s rmp_t;
typedef struct rmp_msg_s rmp_msg_t;

typedef struct p2p_s
{
    struct
    {
        p2p_hal_t hal;
        bool started;
        rmp_t *rmp;
    } internal;
} p2p_t;

void p2p_init(p2p_t *p2p, rmp_t *rmp);
void p2p_start(p2p_t *p2p);
void p2p_stop(p2p_t *p2p);
void p2p_update(p2p_t *p2p);