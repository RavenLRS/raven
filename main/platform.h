#pragma once

#include "platform/platform_macros.h"

#if defined(RAVEN_PLATFORM_ESP32_LORA_TTGO_433_SCREEN)
#include "platform/platforms/esp32_lora_ttgo_433_screen.h"
#elif defined(RAVEN_PLATFORM_ESP32_LORA_TTGO_433)
#include "platform/platforms/esp32_lora_ttgo_433.h"
#elif defined(RAVEN_PLATFORM_ESP32_LORA_TTGO_868_915_SCREEN)
#include "platform/platforms/esp32_lora_ttgo_868_915_screen.h"
#elif defined(RAVEN_PLATFORM_ESP32_LORA_TTGO_868_915)
#include "platform/platforms/esp32_lora_ttgo_868_915.h"
#else
#error Unknown platform
#endif