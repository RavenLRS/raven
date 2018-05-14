#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <hal/storage.h>

typedef struct storage_s
{
    storage_hal_t hal;
} storage_t;

void storage_init(storage_t *storage, const char *name);

bool storage_get_bool(storage_t *storage, const char *key, bool *v);
bool storage_get_u8(storage_t *storage, const char *key, uint8_t *v);
bool storage_get_i8(storage_t *storage, const char *key, int8_t *v);
bool storage_get_u16(storage_t *storage, const char *key, uint16_t *v);
bool storage_get_i16(storage_t *storage, const char *key, int16_t *v);
bool storage_get_u32(storage_t *storage, const char *key, uint32_t *v);
bool storage_get_i32(storage_t *storage, const char *key, int32_t *v);
bool storage_get_str(storage_t *storage, const char *key, char *buf, size_t *size);
bool storage_get_blob(storage_t *storage, const char *key, void *buf, size_t *size);
// Returns true iff blob exists and has exactly the same size
bool storage_get_sized_blob(storage_t *storage, const char *key, void *buf, size_t size);

void storage_set_bool(storage_t *storage, const char *key, bool v);
void storage_set_u8(storage_t *storage, const char *key, uint8_t v);
void storage_set_i8(storage_t *storage, const char *key, int8_t v);
void storage_set_u16(storage_t *storage, const char *key, uint16_t v);
void storage_set_i16(storage_t *storage, const char *key, int16_t v);
void storage_set_u32(storage_t *storage, const char *key, uint32_t v);
void storage_set_i32(storage_t *storage, const char *key, int32_t v);
void storage_set_str(storage_t *storage, const char *key, const char *s);
void storage_set_blob(storage_t *storage, const char *key, const void *buf, size_t size);

void storage_commit(storage_t *storage);
