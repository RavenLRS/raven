#pragma once

#include <hal/mutex_base.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

typedef struct mutex_s
{
    SemaphoreHandle_t sema;
} mutex_t;
