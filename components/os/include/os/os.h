#pragma once

#if defined(USE_FREERTOS_SOURCE)
#include <FreeRTOS.h>
#include <task.h>
// IRAM_ATTR is only defined for ESP32
#define IRAM_ATTR
// FreeRTOS 10 accepts an argument on portYIELD_FROM_ISR()
#define portYIELD_FROM_ISR_IF(x) portYIELD_FROM_ISR(x)
// xTaskCreatePinnedToCore() is ESP32 specific. We don't support
// multiple cores on STM32, so we map it to xTaskCreate()
#define xTaskCreatePinnedToCore(c, n, ss, p, pr, h, cid) xTaskCreate(c, n, ss, p, pr, h)
#else
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
// FreeRTOS 8 in ESP32 accepts no argument on portYIELD_FROM_ISR(),
// so we wrap it in an if
#define portYIELD_FROM_ISR_IF(x)  \
    do                            \
    {                             \
        if (x)                    \
        {                         \
            portYIELD_FROM_ISR(); \
        }                         \
    } while (0)
#endif
