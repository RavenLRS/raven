#pragma once

#include <stdbool.h>

#include <driver/gpio.h>

typedef enum
{
    SYSTEM_FLAG_SCREEN = 1 << 0,  // Screen support is available and screen is detected
    SYSTEM_FLAG_BUTTON = 1 << 1,  // Button is available,
    SYSTEM_FLAG_BATTERY = 1 << 2, // Battery circuitry is available
} system_flag_e;

system_flag_e system_get_flags(void);
system_flag_e system_add_flag(system_flag_e flag);
system_flag_e system_remove_flag(system_flag_e flag);
bool system_has_flag(system_flag_e flag);

bool system_awake_from_deep_sleep(void);
void system_shutdown(void);
