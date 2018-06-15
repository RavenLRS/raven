#include <string.h>

#include <hal/log.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <rom/miniz.h>

#include <esp_ota_ops.h>

#include "util/macros.h"
#include "util/time.h"

#include "ota.h"

#define INFLATOR_BUFSIZE TINFL_LZ_DICT_SIZE
#define OTA_YIELD_AFTER_BYTES 256 // We yield each 256 bytes to avoid stalling other tasks too much

static const char *TAG = "OTA";

typedef struct ota_status_s
{
    uint32_t image_size;
    uint32_t data_size;
    ota_data_format_e data_format;

    time_ticks_t started;
    time_ticks_t estimated_remaining;
    uint32_t offset;
    uint32_t last_yield;
    esp_ota_handle_t handle;
    const esp_partition_t *partition;
    bool got_bad_chunk;
    uint8_t *inflator_out;
    uint8_t *next_inflator_out;
    tinfl_decompressor *inflator;
} ota_status_t;

static ota_status_t status;

void ota_init(void)
{
    memset(&status, 0, sizeof(status));
}

void ota_begin(uint32_t image_size, uint32_t data_size, ota_data_format_e data_format)
{
    esp_err_t err;

    if (status.image_size == image_size && status.data_size == data_size &&
        status.data_format == data_format && status.offset == 0 && status.handle != 0)
    {
        // Already ready for this OTA, no need to tear down and set it up again
        return;
    }

    status.image_size = image_size;
    status.data_size = data_size;
    status.data_format = data_format;
    status.offset = 0;

    if (status.handle != 0)
    {
        // Cancel previous update
        esp_ota_end(status.handle);
    }

    if (status.inflator)
    {
        free(status.inflator);
        status.inflator = NULL;
    }

    if (status.inflator_out)
    {
        free(status.inflator_out);
        status.inflator_out = NULL;
    }

    status.partition = esp_ota_get_next_update_partition(NULL);
    assert(status.partition);
    LOG_I(TAG, "Will write OTA image of size %u to partition %s at offset 0x%x",
          image_size, status.partition->label, status.partition->address);
    err = esp_ota_begin(status.partition, image_size, &status.handle);
    // TODO: Do something better than crashing here
    ESP_ERROR_CHECK(err);
    // XXX: Sometimes we don't have enough heap here. Consider
    // stopping wifi and all related resources, which will free
    // some heap.
    status.inflator = malloc(sizeof(*status.inflator));
    assert(status.inflator);
    tinfl_init(status.inflator);
    status.inflator_out = malloc(INFLATOR_BUFSIZE);
    assert(status.inflator_out);
    status.next_inflator_out = status.inflator_out;

    status.started = time_ticks_now();
    status.estimated_remaining = 0;
}

static esp_err_t ota_write_raw(const void *data, size_t size)
{
    return esp_ota_write(status.handle, data, size);
}

static esp_err_t ota_write_zlib(const void *data, size_t size)
{
    esp_err_t err = ESP_OK;

    mz_uint32 flags = TINFL_FLAG_PARSE_ZLIB_HEADER;
    if (status.offset + size < status.data_size)
    {
        flags |= TINFL_FLAG_HAS_MORE_INPUT;
    }

    // We might need multiple calls to tinfl_decompress() to process all data
    const uint8_t *ptr = data;
    size_t used = 0;
    while (used < size)
    {
        size_t in_bytes = size - used;
        size_t out_bytes = INFLATOR_BUFSIZE - (status.next_inflator_out - status.inflator_out);
        int infl_status = tinfl_decompress(status.inflator, &ptr[used], &in_bytes,
                                           status.inflator_out, status.next_inflator_out,
                                           &out_bytes, flags);

        status.next_inflator_out += out_bytes;
        used += in_bytes;

        if (infl_status < 0)
        {
            LOG_W(TAG, "Error %d inflating data", infl_status);
            break;
        }

        size_t bytes_in_out_buf = status.next_inflator_out - status.inflator_out;
        if (infl_status <= TINFL_STATUS_DONE || bytes_in_out_buf == INFLATOR_BUFSIZE)
        {
            err = esp_ota_write(status.handle, status.inflator_out, bytes_in_out_buf);
            if (err != ESP_OK)
            {
                return err;
            }
            // XXX: Don't zero the buffer, tinfl_decompress() might read back from it
            status.next_inflator_out = status.inflator_out;
        }
    }
    return err;
}

void ota_write(uint32_t data_offset, const void *data, size_t size)
{
    esp_err_t err;

    if (status.handle == 0)
    {
        // OTA not started
        return;
    }

    if (data_offset != status.offset)
    {
        LOG_I(TAG, "Unexpected data offset %u, expecting %u", data_offset, status.offset);
        status.got_bad_chunk = true;
        return;
    }

    status.got_bad_chunk = false;
    switch (status.data_format)
    {
    case OTA_DATA_FORMAT_RAW:
        err = ota_write_raw(data, size);
        break;
    case OTA_DATA_FORMAT_ZLIB:
        err = ota_write_zlib(data, size);
        break;
    default:
        err = ESP_OK;
    }
    ESP_ERROR_CHECK(err);
    status.offset += size;
    LOG_I(TAG, "Wrote up to offset %u", status.offset);
    if (status.offset > status.last_yield + OTA_YIELD_AFTER_BYTES)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
        status.last_yield = status.offset;
    }

    if (status.data_size > 0)
    {
        time_ticks_t elapsed = time_ticks_now() - status.started;
        time_ticks_t remaining = (elapsed / ((float)status.offset / status.data_size)) - elapsed;
        if (status.estimated_remaining == 0)
        {
            status.estimated_remaining = remaining;
        }
        else
        {
            status.estimated_remaining = status.estimated_remaining * 0.95f + remaining * 0.05f;
        }
    }
}

void ota_end(void)
{
    esp_err_t err;

    if (status.handle == 0)
    {
        return;
    }

    // TODO: Do something better than crashing here
    err = esp_ota_end(status.handle);
    ESP_ERROR_CHECK(err);
    err = esp_ota_set_boot_partition(status.partition);
    ESP_ERROR_CHECK(err);
    esp_restart();
}

ota_status_e ota_get_status(uint32_t *data_offset, ota_data_format_e *data_format)
{
    if (data_offset)
    {
        *data_offset = status.offset;
    }
    if (data_format)
    {
        *data_format = status.data_format;
    }
    if (status.handle == 0)
    {
        return OTA_STATUS_IDLE;
    }
    if (status.got_bad_chunk)
    {
        return OTA_STATUS_AWAITING_OFFSET;
    }
    return OTA_STATUS_WROTE_OFFSET;
}

bool ota_is_in_progress(void)
{
    return status.handle > 0;
}

float ota_progress_completion(void)
{
    if (ota_is_in_progress() && status.data_size > 0)
    {
        return (float)status.offset / status.data_size;
    }
    return -1;
}

int ota_estimated_time_remaining(void)
{
    if (ota_is_in_progress())
    {
        return TICKS_TO_MILLIS(status.estimated_remaining) / 1000;
    }
    return -1;
}