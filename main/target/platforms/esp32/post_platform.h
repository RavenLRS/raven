#pragma once

#if defined(HAVE_SDKCONFIG_H)
#include <sdkconfig.h>
#endif

// Default values for some parameters

// If no GPIOs are defined for half duplex,
// use UART0.
#if !defined(TX_UNUSED_GPIO)
#define TX_UNUSED_GPIO 1
#endif
#if !defined(RX_UNUSED_GPIO)
#define RX_UNUSED_GPIO 3
#endif
