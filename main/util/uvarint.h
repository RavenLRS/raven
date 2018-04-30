#pragma once

#include <stddef.h>
#include <stdint.h>

// Returns the number of written bytes or -1 if the number doesn't fit in the buffer
int uvarint_encode16(void *buf, size_t size, uint16_t v);
int uvarint_encode32(void *buf, size_t size, uint32_t v);
// Returns the number of used bytes, or -1 if the data didn't contain a valid uvarint
// of the given size.
int uvarint_decode16(uint16_t *v, const void *data, size_t size);
int uvarint_decode32(uint32_t *v, const void *data, size_t size);