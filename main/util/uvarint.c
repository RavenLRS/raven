#include "macros.h"

#include "uvarint.h"

static int uvarint_encode64(void *buf, size_t size, uint64_t v)
{
    uint8_t *ptr = buf;
    for (int ii = 0; ii < size; ii++)
    {
        if (v < 0x80)
        {
            ptr[ii] = ((uint8_t)v);
            return ii + 1;
        }
        ptr[ii] = ((uint8_t)v) | 0x80;
        v >>= 7;
    }
    return -1;
}

static int uvarint_decode64(uint64_t *v, const void *data, size_t size, size_t type_size)
{
    const uint8_t *ptr = data;
    unsigned s = 0;
    *v = 0;
    // for a type of N bytes we need at most N+1 bytes, since
    // we don't support types of more than 8 bytes
    for (int ii = 0; ii < MIN(size, type_size + 1); ii++)
    {
        uint8_t b = ptr[ii];
        if (b < 0x80)
        {
            *v |= ((uint64_t)b) << s;
            return ii + 1;
        }
        *v |= ((uint64_t)(b & 0x7f)) << s;
        s += 7;
    }
    return -1;
}

int uvarint_encode16(void *buf, size_t size, uint16_t v)
{
    return uvarint_encode64(buf, size, v);
}

int uvarint_encode32(void *buf, size_t size, uint32_t v)
{
    return uvarint_encode64(buf, size, v);
}

int uvarint_decode16(uint16_t *v, const void *data, size_t size)
{
    uint64_t vv;
    int n = uvarint_decode64(&vv, data, size, sizeof(uint16_t));
    if (n > 0)
    {
        *v = (uint16_t)vv;
    }
    return n;
}

int uvarint_decode32(uint32_t *v, const void *data, size_t size)
{
    uint64_t vv;
    int n = uvarint_decode64(&vv, data, size, sizeof(uint32_t));
    if (n > 0)
    {
        *v = (uint32_t)vv;
    }
    return n;
}