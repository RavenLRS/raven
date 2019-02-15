#include <string.h>

#include "storage.h"

void storage_init(storage_t *storage, storage_namespace_e ns)
{
    HAL_ERR_ASSERT_OK(hal_storage_init(&storage->hal, (uint8_t)ns));
}

bool storage_get_bool(storage_t *storage, const void *key, size_t key_size, bool *v)
{
    return storage_get_sized_blob(storage, key, key_size, v, sizeof(*v));
}

bool storage_get_u8(storage_t *storage, const void *key, size_t key_size, uint8_t *v)
{
    return storage_get_sized_blob(storage, key, key_size, v, sizeof(*v));
}

bool storage_get_i8(storage_t *storage, const void *key, size_t key_size, int8_t *v)
{
    return storage_get_sized_blob(storage, key, key_size, v, sizeof(*v));
}

bool storage_get_u16(storage_t *storage, const void *key, size_t key_size, uint16_t *v)
{
    return storage_get_sized_blob(storage, key, key_size, v, sizeof(*v));
}

bool storage_get_i16(storage_t *storage, const void *key, size_t key_size, int16_t *v)
{
    return storage_get_sized_blob(storage, key, key_size, v, sizeof(*v));
}

bool storage_get_u32(storage_t *storage, const void *key, size_t key_size, uint32_t *v)
{
    return storage_get_sized_blob(storage, key, key_size, v, sizeof(*v));
}

bool storage_get_i32(storage_t *storage, const void *key, size_t key_size, int32_t *v)
{
    return storage_get_sized_blob(storage, key, key_size, v, sizeof(*v));
}

bool storage_get_str(storage_t *storage, const void *key, size_t key_size, char *buf, size_t *size)
{
    bool found;
    HAL_ERR_ASSERT_OK(hal_storage_get_blob(&storage->hal, key, key_size, buf, size, &found));
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

bool storage_get_blob(storage_t *storage, const void *key, size_t key_size, void *buf, size_t *size)
{
    bool found;
    HAL_ERR_ASSERT_OK(hal_storage_get_blob(&storage->hal, key, key_size, buf, size, &found));
    return found;
}

bool storage_get_sized_blob(storage_t *storage, const void *key, size_t key_size, void *buf, size_t size)
{
    size_t blob_size = size;
    bool found;
    HAL_ERR_ASSERT_OK(hal_storage_get_blob(&storage->hal, key, key_size, buf, &blob_size, &found));
    return found && blob_size == size;
}

void storage_set_bool(storage_t *storage, const void *key, size_t key_size, bool v)
{
    hal_storage_set_blob(&storage->hal, key, key_size, &v, sizeof(v));
}

void storage_set_u8(storage_t *storage, const void *key, size_t key_size, uint8_t v)
{
    hal_storage_set_blob(&storage->hal, key, key_size, &v, sizeof(v));
}

void storage_set_i8(storage_t *storage, const void *key, size_t key_size, int8_t v)
{
    hal_storage_set_blob(&storage->hal, key, key_size, &v, sizeof(v));
}

void storage_set_u16(storage_t *storage, const void *key, size_t key_size, uint16_t v)
{
    hal_storage_set_blob(&storage->hal, key, key_size, &v, sizeof(v));
}

void storage_set_i16(storage_t *storage, const void *key, size_t key_size, int16_t v)
{
    hal_storage_set_blob(&storage->hal, key, key_size, &v, sizeof(v));
}

void storage_set_u32(storage_t *storage, const void *key, size_t key_size, uint32_t v)
{
    hal_storage_set_blob(&storage->hal, key, key_size, &v, sizeof(v));
}

void storage_set_i32(storage_t *storage, const void *key, size_t key_size, int32_t v)
{
    hal_storage_set_blob(&storage->hal, key, key_size, &v, sizeof(v));
}

void storage_set_str(storage_t *storage, const void *key, size_t key_size, const char *s)
{
    if (!s)
    {
        s = "";
    }
    hal_storage_set_blob(&storage->hal, key, key_size, s, strlen(s) + 1);
}

void storage_set_blob(storage_t *storage, const void *key, size_t key_size, const void *buf, size_t size)
{
    hal_storage_set_blob(&storage->hal, key, key_size, buf, size);
}

void storage_commit(storage_t *storage)
{
    HAL_ERR_ASSERT_OK(hal_storage_commit(&storage->hal));
}