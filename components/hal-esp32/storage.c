#include <hal/log.h>
#include <hal/storage.h>

LOG_TAG_DECLARE("HAL.Storage");

#define HAL_STORAGE_SKEY_SIZE 16

static void hal_storage_format_skey(hal_storage_t *s, char *buf, const void *key, size_t key_size)
{
    (void)s;

    const uint8_t *p = key;
    const uint8_t *pend = p + key_size;
    char *bufend = buf + HAL_STORAGE_SKEY_SIZE;
    for (; p != pend && buf < bufend; p++, buf++)
    {
        if (*p)
        {
            *buf = *p;
        }
        else
        {
            *buf = 0xFF;
            buf++;
            if (buf < bufend)
            {
                *buf = 0xFF;
            }
        }
    }
    if (buf >= bufend)
    {
        LOG_F(TAG, "Increase HAL_STORAGE_SKEY_SIZE (%d)", HAL_STORAGE_SKEY_SIZE);
    }
    *buf = '\0';
}

hal_err_t hal_storage_init(hal_storage_t *s, uint8_t tag)
{
    esp_err_t err;
    char name[] = {tag, '\0'};

    if ((err = nvs_flash_init()) != ESP_OK)
    {
        return err;
    }
    if ((err = nvs_open(name, NVS_READWRITE, &s->handle)) != ESP_OK)
    {
        return err;
    }
    return ESP_OK;
}

hal_err_t hal_storage_get_blob(hal_storage_t *s, const void *key, size_t key_size, void *buf, size_t *size, bool *found)
{
    char skey[HAL_STORAGE_SKEY_SIZE];
    hal_storage_format_skey(s, skey, key, key_size);

    esp_err_t err = nvs_get_blob(s->handle, skey, buf, size);
    if (err == ESP_OK)
    {
        if (found)
        {
            *found = true;
        }
        return ESP_OK;
    }
    if (found)
    {
        *found = false;
    }
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        return ESP_OK;
    }
    return err;
}

hal_err_t hal_storage_set_blob(hal_storage_t *s, const void *key, size_t key_size, const void *buf, size_t size)
{
    char skey[HAL_STORAGE_SKEY_SIZE];
    hal_storage_format_skey(s, skey, key, key_size);

    if (size > 0)
    {
        return nvs_set_blob(s->handle, skey, buf, size);
    }

    esp_err_t err = nvs_erase_key(s->handle, skey);
    if (err != ESP_ERR_NVS_NOT_FOUND)
    {
        return err;
    }
    return ESP_OK;
}

hal_err_t hal_storage_commit(hal_storage_t *s)
{
    return nvs_commit(s->handle);
}
