#pragma once

#if defined(ESP32)
#include "target/platforms/esp32/pre_platform.h"
#endif

#if defined(STM32F1)
#include "target/platforms/stm32/f1/pre_platform.h"
#endif

#include "platform.h"

#if defined(ESP32)
#include "target/platforms/esp32/post_platform.h"
#endif

#if defined(STM32F1)
#include "target/platforms/stm32/f1/post_platform.h"
#endif