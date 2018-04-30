#include <nvs_flash.h>

#include <hal/storage.h>

void storage_hal_init(storage_hal_t *storage_hal, const char *name)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(nvs_open(name, NVS_READWRITE, &storage_hal->nvs_handle));
}

bool storage_hal_get_blob(storage_hal_t *storage_hal, const char *key, void *buf, size_t *size)
{
    esp_err_t err = nvs_get_blob(storage_hal->nvs_handle, key, buf, size);
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

void storage_hal_set_blob(storage_hal_t *storage_hal, const char *key, const void *buf, size_t size)
{
    if (size > 0)
    {
        ESP_ERROR_CHECK(nvs_set_blob(storage_hal->nvs_handle, key, buf, size));
    }
    else
    {
        esp_err_t err = nvs_erase_key(storage_hal->nvs_handle, key);
        if (err != ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_ERROR_CHECK(err);
        }
    }
}

void storage_hal_commit(storage_hal_t *storage_hal)
{
    ESP_ERROR_CHECK(nvs_commit(storage_hal->nvs_handle));
}