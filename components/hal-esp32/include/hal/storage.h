#pragma once

#include <hal/storage_base.h>

#include <nvs_flash.h>

typedef struct hal_storage_s
{
    nvs_handle handle;
} hal_storage_t;