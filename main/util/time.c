
#include <sys/time.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_attr.h>
#include <esp_timer.h>

#include "time.h"

unsigned long IRAM_ATTR millis(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

bool millis_ellapsed(unsigned long since_ms, unsigned long now_ms, unsigned long interval_ms)
{
    if (now_ms == 0)
    {
        now_ms = millis();
    }
    return now_ms - since_ms > interval_ms;
}

bool millis_ellapsed_secs(unsigned long since_ms, unsigned long now_ms, float interval_secs)
{
    return millis_ellapsed(since_ms, now_ms, interval_secs * MILLIS_PER_SEC);
}
