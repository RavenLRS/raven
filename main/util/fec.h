#pragma once

#include <stddef.h>

#define FEC_ENCODED_SIZE(x) (2 * x)
#define FEC_DECODED_SIZE(x) (x / 2)

size_t fec_encode(const void *data, size_t size, void *output, size_t output_size);
size_t fec_decode(const void *data, size_t size, void *output, size_t output_size);