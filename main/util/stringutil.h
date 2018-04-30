#pragma once

#include <stddef.h>
#include <string.h>

// Copies src into dst, using at most dst_size bytes and guarantees
// dst is NULL-terminated. Returns the number of written bytes including
// the '\0' terminator.
inline size_t strput(char *dst, const char *src, size_t dst_size)
{
    size_t r = strlcpy(dst, src, dst_size) + 1;
    return r < dst_size ? r : dst_size;
}