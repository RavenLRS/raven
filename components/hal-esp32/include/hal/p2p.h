#pragma once

#include <stddef.h>

typedef struct p2p_hal_s p2p_hal_t;

typedef void (*p2p_hal_callback_f)(p2p_hal_t *p2p_hal, const void *data, size_t size, void *user_data);

typedef struct p2p_hal_s
{
    p2p_hal_callback_f callback;
    void *user_data;
} p2p_hal_t;

void p2p_hal_init(p2p_hal_t *hal, p2p_hal_callback_f callback, void *user_data);
void p2p_hal_start(p2p_hal_t *hal);
void p2p_hal_stop(p2p_hal_t *hal);
void p2p_hal_broadcast(p2p_hal_t *hal, const void *data, size_t size);