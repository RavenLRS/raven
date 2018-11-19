#pragma once

#include <stdint.h>

inline uint64_t hal_time_micros_now(void)
{
#warning this might lead to some stalling
    return 0;
}
