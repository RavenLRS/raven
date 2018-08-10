#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "util/time.h"

#define IO_MAKE(r, w, f, d) ((io_t){ \
    .read = (io_read_f)r,            \
    .write = (io_write_f)w,          \
    .flags = (io_flags_f)f,          \
    .data = d,                       \
})

typedef enum
{
    IO_FLAG_HALF_DUPLEX = 1 << 0,
} io_flags_t;

typedef int (*io_read_f)(void *data, void *buf, size_t size, time_ticks_t timeout);
typedef int (*io_write_f)(void *data, const void *buf, size_t size);
typedef io_flags_t (*io_flags_f)(void *data);

typedef struct io_s
{
    io_read_f read;
    io_write_f write;
    io_flags_f flags;
    void *data;
} io_t;

void io_init(io_t *io, io_read_f r, io_write_f w, io_flags_f f, void *data);
int io_read(io_t *io, void *buf, size_t size, time_ticks_t timeout);
int io_write(io_t *io, const void *buf, size_t size);

io_flags_t io_get_flags(io_t *io);
bool io_is_half_duplex(io_t *io);
