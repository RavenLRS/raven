#pragma once

#include <stdint.h>

#include <hal/md5_base.h>

// MD5 context.
typedef struct
{
    uint32_t state[4];  // state (ABCD)
    uint32_t count[2];  // number of bits, modulo 2^64 (lsb first)
    uint8_t buffer[64]; // input buffer
} MD5_CTX;

typedef struct hal_md5_ctx_s
{
    MD5_CTX ctx;
} hal_md5_ctx_t;