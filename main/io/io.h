#pragma once

#include <stddef.h>

#include "util/time.h"

typedef int (*io_read_f)(void *data, void *buf, size_t size, time_ticks_t timeout);
typedef int (*io_write_f)(void *data, const void *buf, size_t size);

typedef struct io_s
{
    io_read_f read;
    io_write_f write;
    void *data;
} io_t;

void io_init(io_t *io, io_read_f r, io_write_f w, void *data);
int io_read(io_t *io, void *buf, size_t size, time_ticks_t timeout);
int io_write(io_t *io, const void *buf, size_t size);
