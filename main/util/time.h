#pragma once

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_timer.h>

#define MILLIS_PER_SEC (1000)

typedef TickType_t time_ticks_t;
typedef uint64_t time_micros_t;

#define TIME_MICROS_MAX UINT64_MAX

#define MILLIS_TO_TICKS(ms) ((ms) / portTICK_PERIOD_MS)
#define SECS_TO_TICKS(s) MILLIS_TO_TICKS(1000 * s)
#define FREQ_TO_TICKS(hz) MILLIS_TO_TICKS(1000 / hz)

#define MILLIS_TO_MICROS(ms) (ms * 1000)
#define SECS_TO_MICROS(s) MILLIS_TO_MICROS(s * 1000)
#define FREQ_TO_MICROS(hz) MILLIS_TO_TICKS(1000000 / hz)

#define TIME_CYCLE_EVERY_MS(ms, n) (((time_ticks_now() * portTICK_PERIOD_MS) / ms) % n)

inline time_ticks_t time_ticks_now(void)
{
    return xTaskGetTickCount();
}

inline void time_millis_delay(unsigned ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

inline bool time_ticks_ellapsed(time_ticks_t since, time_ticks_t now, time_ticks_t duration)
{
    return since == 0 || now - since >= duration;
}

inline time_micros_t time_micros_now(void) { return esp_timer_get_time(); }
inline void time_micros_delay(time_micros_t delay)
{
    time_micros_t end = time_micros_now() + delay;
    while (time_micros_now() < end)
    {
    }
}

unsigned long millis(void);
bool millis_ellapsed(unsigned long since_ms, unsigned long now_ms, unsigned long interval_ms);
bool millis_ellapsed_secs(unsigned long since_ms, unsigned long now_ms, float interval_secs);
