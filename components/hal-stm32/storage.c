#include <hal/storage.h>

// TODO: Whole file

void hal_storage_init(hal_storage_t *s, const char *name)
{
}

bool hal_storage_get_blob(hal_storage_t *s, const char *key, void *buf, size_t *size)
{
    return false;
}

void hal_storage_set_blob(hal_storage_t *s, const char *key, const void *buf, size_t size)
{
}

void hal_storage_commit(hal_storage_t *s)
{
}