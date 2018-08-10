#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

typedef struct mutex_s
{
    SemaphoreHandle_t sema;
} mutex_t;

void mutex_open(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);
void mutex_close(mutex_t *mutex);