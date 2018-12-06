#pragma once

#include <hal/mutex_base.h>

typedef struct mutex_s
{
    // NOP on STM32, since we don't support multicore
} mutex_t;