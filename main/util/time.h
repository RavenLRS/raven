#pragma once

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#include <os/os.h>

#include <hal/time.h>

#define MILLIS_PER_SEC (1000)
#define MICROS_PER_SEC (1000000)

typedef TickType_t time_ticks_t;
typedef uint32_t time_millis_t;
typedef uint64_t time_micros_t;

#define TIME_MICROS_MAX UINT64_MAX

#define MILLIS_TO_TICKS(ms) ((ms) / portTICK_PERIOD_MS)
#define SECS_TO_TICKS(s) MILLIS_TO_TICKS(1000 * s)
#define FREQ_TO_TICKS(hz) MILLIS_TO_TICKS(1000 / hz)
#define TICKS_TO_MILLIS(t) (t * portTICK_PERIOD_MS)

#define MILLIS_TO_MICROS(ms) (ms * 1000)
#define SECS_TO_MICROS(s) MILLIS_TO_MICROS(s * 1000)
#define FREQ_TO_MICROS(hz) MILLIS_TO_TICKS(1000000 / hz)

#define TIME_CYCLE_EVERY_MS(ms, n) (((time_ticks_now() * portTICK_PERIOD_MS) / ms) % n)

inline time_ticks_t time_ticks_now(void)
{
    return xTaskGetTickCount();
}

inline time_millis_t time_millis_now(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

inline void time_millis_delay(unsigned ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

inline bool time_ticks_ellapsed(time_ticks_t since, time_ticks_t now, time_ticks_t duration)
{
    return since == 0 || now - since >= duration;
}

inline time_micros_t time_micros_now(void)
{
    return hal_time_micros_now();
}

inline void time_micros_delay(time_micros_t delay)
{
    time_micros_t end = time_micros_now() + delay;
    while (time_micros_now() < end)
    {
    }
}
