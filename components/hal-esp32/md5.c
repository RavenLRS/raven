#include <hal/md5.h>

void hal_md5_init(hal_md5_ctx_t *ctx)
{
    mbedtls_md5_init(&ctx->ctx);
    mbedtls_md5_starts(&ctx->ctx);
}

void hal_md5_update(hal_md5_ctx_t *ctx, const void *input, size_t size)
{
    mbedtls_md5_update(&ctx->ctx, (unsigned char *)input, size);
}

void hal_md5_digest(hal_md5_ctx_t *ctx, uint8_t output[HAL_MD5_OUTPUT_SIZE])
{
    mbedtls_md5_finish(&ctx->ctx, output);
}

void hal_md5_destroy(hal_md5_ctx_t *ctx)
{
    mbedtls_md5_free(&ctx->ctx);
}