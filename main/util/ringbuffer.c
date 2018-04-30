#include <string.h>

#include "ringbuffer.h"

bool ring_buffer_really_push(ring_buffer_t *rb, const void *item, bool force)
{
    if (rb->count == rb->capacity)
    {
        if (!force)
        {
            return false;
        }
        ring_buffer_discard(rb);
    }
    memcpy(rb->head, item, rb->sz);
    rb->head = (char *)rb->head + rb->sz;
    if (rb->head == rb->buffer_end)
    {
        rb->head = rb->buffer;
    }
    rb->count++;
    return true;
}

bool ring_buffer_push(ring_buffer_t *rb, const void *item)
{
    return ring_buffer_really_push(rb, item, false);
}

bool ring_buffer_force_push(ring_buffer_t *rb, const void *item)
{
    return ring_buffer_really_push(rb, item, true);
}

bool ring_buffer_pop(ring_buffer_t *rb, void *item)
{
    if (!ring_buffer_peek(rb, item))
    {
        return false;
    }
    return ring_buffer_discard(rb);
}

bool ring_buffer_peek(ring_buffer_t *rb, void *item)
{
    if (rb->count == 0)
    {
        return false;
    }
    if (item != NULL)
    {
        memcpy(item, rb->tail, rb->sz);
    }
    return true;
}

bool ring_buffer_discard(ring_buffer_t *rb)
{
    if (rb->count == 0)
    {
        return false;
    }
    rb->tail = (char *)rb->tail + rb->sz;
    if (rb->tail == rb->buffer_end)
    {
        rb->tail = rb->buffer;
    }
    rb->count--;
    return true;
}

void ring_buffer_empty(ring_buffer_t *rb)
{
    while (ring_buffer_discard(rb))
    {
    }
}

size_t ring_buffer_count(const ring_buffer_t *rb)
{
    return rb->count;
}