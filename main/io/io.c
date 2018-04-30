#include "io.h"

void io_init(io_t *io, io_read_f r, io_write_f w, void *data)
{
    io->read = r;
    io->write = w;
    io->data = data;
}

int io_read(io_t *io, void *buf, size_t size, time_ticks_t timeout)
{
    if (!io->read)
    {
        return -1;
    }
    return io->read(io->data, buf, size, timeout);
}

int io_write(io_t *io, const void *buf, size_t size)
{
    if (!io->write)
    {
        return -1;
    }
    return io->write(io->data, buf, size);
}
