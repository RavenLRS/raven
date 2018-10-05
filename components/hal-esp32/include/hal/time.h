#pragma once

#include <stdint.h>

#include <esp_timer.h>

inline uint64_t hal_time_micros_now(void)
{
    return esp_timer_get_time();
}