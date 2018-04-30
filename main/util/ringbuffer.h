#pragma once

#include <stdbool.h>
#include <stddef.h>

#define RING_BUFFER_DECLARE(name, typ, cap)         \
    struct __attribute__((packed))                  \
    {                                               \
        ring_buffer_t name;                         \
        char __##name##_backing[sizeof(typ) * cap]; \
    }

#define RING_BUFFER_DECLARE_VAR(name, field_name, typ, cap) \
    RING_BUFFER_DECLARE(field_name, typ, cap)               \
    name;

#define RING_BUFFER_INIT(rb, typ, cap)                           \
    do                                                           \
    {                                                            \
        (rb)->buffer = (rb)->buffer_ptr;                         \
        (rb)->buffer_end = (rb)->buffer_ptr + sizeof(typ) * cap; \
        (rb)->capacity = cap;                                    \
        (rb)->count = 0;                                         \
        (rb)->sz = sizeof(typ);                                  \
        (rb)->head = (rb)->buffer;                               \
        (rb)->tail = (rb)->buffer;                               \
    } while (0)

typedef struct ring_buffer_s
{
    void *buffer;
    void *buffer_end;
    size_t capacity;
    size_t count;
    size_t sz;
    void *head;
    void *tail;
    unsigned char buffer_ptr[];
} ring_buffer_t;

bool ring_buffer_push(ring_buffer_t *rb, const void *item);
bool ring_buffer_force_push(ring_buffer_t *rb, const void *item);
bool ring_buffer_pop(ring_buffer_t *rb, void *item);
bool ring_buffer_peek(ring_buffer_t *rb, void *item);
bool ring_buffer_discard(ring_buffer_t *rb);
void ring_buffer_empty(ring_buffer_t *rb);
size_t ring_buffer_count(const ring_buffer_t *rb);
