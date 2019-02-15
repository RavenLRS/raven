#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <wldb.h>

#include <hal/log.h>
#include <hal/rand.h>
#include <hal/storage.h>

#if !defined(CONFIG_PAGE_SIZE)
#error Missing CONFIG_PAGE_SIZE
#endif

#if !defined(CONFIG_BLOCK_SIZE)
#error Missing CONFIG_BLOCK_SIZE
#endif

LOG_TAG_DECLARE("HAL.Storage");

#define KEY_SIZE_MAX 8
#define WLDB_RET_TO_HAL(ret) (ret < 0 ? ret : HAL_ERR_NONE)

static wldb_t db;

extern uint8_t __storage_start;
extern uint8_t __storage_end;

static size_t hal_storage_format_key(hal_storage_t *s, const void *key, size_t key_size, uint8_t *buf)
{
    if (key_size > KEY_SIZE_MAX - 1)
    {
        LOG_F(TAG, "Key size %u > %d", (unsigned)key_size, WLDB_KEY_SIZE_MAX);
    }
    buf[0] = s->ns;
    memcpy(&buf[1], key, key_size);
    return key_size + 1;
}

hal_err_t hal_storage_init(hal_storage_t *s, uint8_t ns)
{
    static bool db_is_initialized = false;
    if (!db_is_initialized)
    {
        int ret = wldb_init(&db, (wldb_addr_t)&__storage_start, (wldb_addr_t)&__storage_end, CONFIG_PAGE_SIZE, CONFIG_BLOCK_SIZE);
        if (ret < 0)
        {
            return ret;
        }
        db_is_initialized = true;
    }
    s->ns = ns;
    return HAL_ERR_NONE;
}

hal_err_t hal_storage_get_blob(hal_storage_t *s, const void *key, size_t key_size, void *buf, size_t *size, bool *found)
{
    uint8_t wkey[KEY_SIZE_MAX];
    size_t ks = hal_storage_format_key(s, key, key_size, wkey);
    int ret = wldb_get(&db, wkey, ks, buf, size);
    if (found)
    {
        *found = ret > 0;
    }
    return WLDB_RET_TO_HAL(ret);
}

hal_err_t hal_storage_set_blob(hal_storage_t *s, const void *key, size_t key_size, const void *buf, size_t size)
{
    uint8_t wkey[KEY_SIZE_MAX];
    size_t ks = hal_storage_format_key(s, key, key_size, wkey);
    int ret = wldb_set(&db, wkey, ks, buf, size);
    return WLDB_RET_TO_HAL(ret);
}

hal_err_t hal_storage_commit(hal_storage_t *s)
{
    (void)s;

    int ret = wldb_commit(&db);
    return WLDB_RET_TO_HAL(ret);
}
