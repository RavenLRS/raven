#pragma once

#include <stdint.h>

inline uint64_t hal_time_micros_now(void)
{
    // Implemented in main.c, since it needs to
    // take into account the app configuration
    extern uint64_t stm32_time_micros_now(void);
    return stm32_time_micros_now();
}
