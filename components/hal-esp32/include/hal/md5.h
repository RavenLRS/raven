#pragma once

#include <mbedtls/md5.h>

#include <hal/md5_base.h>

typedef struct hal_md5_ctx_s
{
    mbedtls_md5_context ctx;
} hal_md5_ctx_t;