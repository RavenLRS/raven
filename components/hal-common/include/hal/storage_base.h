#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <hal/err.h>

typedef struct hal_storage_s hal_storage_t;

hal_err_t hal_storage_init(hal_storage_t *s, uint8_t tag);
hal_err_t hal_storage_get_blob(hal_storage_t *s, const void *key, size_t key_size, void *buf, size_t *size, bool *found);
hal_err_t hal_storage_set_blob(hal_storage_t *s, const void *key, size_t key_size, const void *buf, size_t size);
hal_err_t hal_storage_commit(hal_storage_t *s);