#include <string.h>

#include "storage.h"

void storage_init(storage_t *storage, const char *name)
{
    hal_storage_init(&storage->hal, name);
}

bool storage_get_bool(storage_t *storage, const char *key, bool *v)
{
    return storage_get_sized_blob(storage, key, v, sizeof(*v));
}

bool storage_get_u8(storage_t *storage, const char *key, uint8_t *v)
{
    return storage_get_sized_blob(storage, key, v, sizeof(*v));
}

bool storage_get_i8(storage_t *storage, const char *key, int8_t *v)
{
    return storage_get_sized_blob(storage, key, v, sizeof(*v));
}

bool storage_get_u16(storage_t *storage, const char *key, uint16_t *v)
{
    return storage_get_sized_blob(storage, key, v, sizeof(*v));
}

bool storage_get_i16(storage_t *storage, const char *key, int16_t *v)
{
    return storage_get_sized_blob(storage, key, v, sizeof(*v));
}

bool storage_get_u32(storage_t *storage, const char *key, uint32_t *v)
{
    return storage_get_sized_blob(storage, key, v, sizeof(*v));
}

bool storage_get_i32(storage_t *storage, const char *key, int32_t *v)
{
    return storage_get_sized_blob(storage, key, v, sizeof(*v));
}

bool storage_get_str(storage_t *storage, const char *key, char *buf, size_t *size)
{
    bool found = hal_storage_get_blob(&storage->hal, key, buf, size);
    if (found)
    {
        // Make sure the string is null-terminated
        if (*size > 0)
        {
            buf[*size - 1] = '\0';
        }
    }
    return found;
}

bool storage_get_blob(storage_t *storage, const char *key, void *buf, size_t *size)
{
    return hal_storage_get_blob(&storage->hal, key, buf, size);
}

bool storage_get_sized_blob(storage_t *storage, const char *key, void *buf, size_t size)
{
    size_t blob_size = size;
    bool ok = hal_storage_get_blob(&storage->hal, key, buf, &blob_size);
    return ok && blob_size == size;
}

void storage_set_bool(storage_t *storage, const char *key, bool v)
{
    hal_storage_set_blob(&storage->hal, key, &v, sizeof(v));
}

void storage_set_u8(storage_t *storage, const char *key, uint8_t v)
{
    hal_storage_set_blob(&storage->hal, key, &v, sizeof(v));
}

void storage_set_i8(storage_t *storage, const char *key, int8_t v)
{
    hal_storage_set_blob(&storage->hal, key, &v, sizeof(v));
}

void storage_set_u16(storage_t *storage, const char *key, uint16_t v)
{
    hal_storage_set_blob(&storage->hal, key, &v, sizeof(v));
}

void storage_set_i16(storage_t *storage, const char *key, int16_t v)
{
    hal_storage_set_blob(&storage->hal, key, &v, sizeof(v));
}

void storage_set_u32(storage_t *storage, const char *key, uint32_t v)
{
    hal_storage_set_blob(&storage->hal, key, &v, sizeof(v));
}

void storage_set_i32(storage_t *storage, const char *key, int32_t v)
{
    hal_storage_set_blob(&storage->hal, key, &v, sizeof(v));
}

void storage_set_str(storage_t *storage, const char *key, const char *s)
{
    if (!s)
    {
        s = "";
    }
    hal_storage_set_blob(&storage->hal, key, s, strlen(s) + 1);
}

void storage_set_blob(storage_t *storage, const char *key, const void *buf, size_t size)
{
    hal_storage_set_blob(&storage->hal, key, buf, size);
}

void storage_commit(storage_t *storage)
{
    hal_storage_commit(&storage->hal);
}