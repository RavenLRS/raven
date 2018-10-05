#include <hal/storage.h>

void hal_storage_init(hal_storage_t *s, const char *name)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_open(name, NVS_READWRITE, &s->handle));
}

bool hal_storage_get_blob(hal_storage_t *s, const char *key, void *buf, size_t *size)
{
    esp_err_t err = nvs_get_blob(s->handle, key, buf, size);
    if (err == ESP_OK)
    {
        return true;
    }
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        return false;
    }
    ESP_ERROR_CHECK(err);
    return false;
}

void hal_storage_set_blob(hal_storage_t *s, const char *key, const void *buf, size_t size)
{
    if (size > 0)
    {
        ESP_ERROR_CHECK(nvs_set_blob(s->handle, key, buf, size));
    }
    else
    {
        esp_err_t err = nvs_erase_key(s->handle, key);
        if (err != ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_ERROR_CHECK(err);
        }
    }
}

void hal_storage_commit(hal_storage_t *s)
{
    ESP_ERROR_CHECK(nvs_commit(s->handle));
}