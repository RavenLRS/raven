#pragma once

#include <stddef.h>
#include <stdint.h>

#define HAL_MD5_OUTPUT_SIZE 16

typedef struct hal_md5_ctx_s hal_md5_ctx_t;

void hal_md5_init(hal_md5_ctx_t *ctx);
void hal_md5_update(hal_md5_ctx_t *ctx, const void *input, size_t size);
void hal_md5_digest(hal_md5_ctx_t *ctx, uint8_t output[HAL_MD5_OUTPUT_SIZE]);
void hal_md5_destroy(hal_md5_ctx_t *ctx);
