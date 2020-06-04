#include <hal/log.h>
#include <hal/storage.h>

#define HAL_STORAGE_SKEY_SIZE 4

static void hal_storage_format_skey(hal_storage_t *s, char *buf, const void *key, size_t key_size)
{
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
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        if (found)
        {
            *found = false;
        }
        return false;
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
