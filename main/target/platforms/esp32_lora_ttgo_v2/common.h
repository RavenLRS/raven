#include "platform/platform_macros.h"

// All v2 boards are wired in the same way, only the LoRa circuitry changes

// V2 boards have no programmable LED. There's a green one
// but it's connected to OLED's SCL (22).

// User button. Since boards ship without a button, we also support using them as touchpad.
#if defined(PIN_BUTTON_TOUCH)
#define PIN_BUTTON_1 PIN_BUTTON_TOUCH
#define PIN_BUTTON_1_IS_TOUCH
#else
#define PIN_BUTTON_1 0
#endif

// Buzzer
#define PIN_BEEPER 12 // This seems appropriate for v1 boards, since it can't be pulled low during boot

// SX127X
#define PIN_SX127X_SCK 5
#define PIN_SX127X_MISO 19
#define PIN_SX127X_MOSI 27
#define PIN_SX127X_CS 18
#define PIN_SX127X_RST 14
#define PIN_SX127X_DIO0 26

#define SX127X_OUTPUT_TYPE SX127X_OUTPUT_PA_BOOST

// SDCard pins are mapped as usable output pins, since we don't use the SDCard right now
// Note that these boards use ESP32-PICO-D4, which has pins 16 and 17 connected to the
// internal SPI flash, so we can't use them.
#define PIN_USABLE_BASE_MASK (PIN_N(1) | PIN_N(3) | PIN_N(4) | PIN_N(11) | PIN_N(13) | PIN_N(23) | PIN_N(25) | PIN_N(32) | PIN_N(33))
#if defined(PIN_BUTTON_TOUCH)
#define PIN_USABLE_MASK (PIN_USABLE_BASE_MASK & ~PIN_N(PIN_BUTTON_TOUCH))
#else
#define PIN_USABLE_MASK PIN_USABLE_BASE_MASK
#endif

#define PIN_DEFAULT_TX 13
#define PIN_DEFAULT_RX 23

#define PIN_UNUSED_TX 2
#define PIN_UNUSED_RX 35
