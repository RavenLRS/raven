#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    OTA_STATUS_IDLE = 0,
    OTA_STATUS_WROTE_OFFSET = 1,
    OTA_STATUS_AWAITING_OFFSET = 2,
} ota_status_e;

typedef enum
{
    OTA_DATA_FORMAT_RAW = 0,
    OTA_DATA_FORMAT_ZLIB = 1,
} ota_data_format_e;

void ota_init(void);
void ota_begin(uint32_t image_size, uint32_t data_size, ota_data_format_e data_format);
void ota_write(uint32_t data_offset, const void *data, size_t size);
ota_status_e ota_get_status(uint32_t *data_offset, ota_data_format_e *data_format);
void ota_end(void);

// Information
bool ota_is_in_progress(void);
// Return the completion in the [0, 1] interval. A return value < 0
// indicates unknown.
float ota_progress_completion(void);
// Returns the estimated number of seconds remaining until OTA completes.
// A return value < 0 indicates unknown.
int ota_estimated_time_remaining(void);