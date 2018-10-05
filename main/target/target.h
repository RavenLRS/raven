#pragma once

#if defined(ESP32)
// All ESP32 targets support these features
#define USE_P2P
#define USE_BLUETOOTH
#define USE_OTA
#endif

#include "platform.h"

#if defined(HAVE_SDKCONFIG_H)
#include <sdkconfig.h>
#endif
