#pragma once

#include <stddef.h>

// Copies src into dst, using at most dst_size bytes and guarantees
// dst is NULL-terminated. Returns the number of written bytes including
// the '\0' terminator.
size_t strput(char *dst, const char *src, size_t dst_size);