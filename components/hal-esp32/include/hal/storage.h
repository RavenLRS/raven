#pragma once

#include <stdint.h>

typedef struct storage_hal_s
{
    uint32_t nvs_handle;
} storage_hal_t;

void storage_hal_init(storage_hal_t *storage_hal, const char *name);
bool storage_hal_get_blob(storage_hal_t *storage_hal, const char *key, void *buf, size_t *size);
void storage_hal_set_blob(storage_hal_t *storage_hal, const char *key, const void *buf, size_t size);
void storage_hal_commit(storage_hal_t *storage_hal);