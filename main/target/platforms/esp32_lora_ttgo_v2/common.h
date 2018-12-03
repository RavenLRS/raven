#include <hal/gpio.h>

// All v2 boards are wired in the same way, only the LoRa circuitry changes

// V2 boards have no programmable LED. There's a green one
// but it's connected to OLED's SCL (22).

// User button. Since boards ship without a button, we also support using them as touchpad.
#if defined(BUTTON_1_TOUCH_GPIO)
#define USE_TOUCH_BUTTON
#define BUTTON_1_GPIO BUTTON_1_TOUCH_GPIO
#define BUTTON_1_GPIO_IS_TOUCH
#else
#define BUTTON_1_GPIO 0
#endif

// Buzzer
#define BEEPER_GPIO 12 // This seems appropriate for v1 boards, since it can't be pulled low during boot

// SX127X
#define USE_RADIO_SX127X
#define SX127X_SPI_BUS VSPI_HOST
#define SX127X_GPIO_SCK 5
#define SX127X_GPIO_MISO 19
#define SX127X_GPIO_MOSI 27
#define SX127X_GPIO_CS 18
#define SX127X_GPIO_RST 14
#define SX127X_GPIO_DIO0 26

#define SX127X_OUTPUT_TYPE SX127X_OUTPUT_PA_BOOST

// SDCard pins are mapped as usable output pins, since we don't use the SDCard right now
// Note that these boards use ESP32-PICO-D4, which has GPIO 16 and 17 connected to the
// internal SPI flash, so we can't use them.
#define HAL_GPIO_USER_BASE_MASK (HAL_GPIO_M(1) | HAL_GPIO_M(3) | HAL_GPIO_M(4) | HAL_GPIO_M(11) | HAL_GPIO_M(13) | HAL_GPIO_M(23) | HAL_GPIO_M(25) | HAL_GPIO_M(32) | HAL_GPIO_M(33))
#if defined(BUTTON_1_TOUCH_GPIO)
#define HAL_GPIO_USER_MASK (HAL_GPIO_USER_BASE_MASK & ~HAL_GPIO_M(BUTTON_1_TOUCH_GPIO))
#else
#define HAL_GPIO_USER_MASK HAL_GPIO_USER_BASE_MASK
#endif

#define TX_DEFAULT_GPIO 13
#define RX_DEFAULT_GPIO 23

#define TX_UNUSED_GPIO 2
#define RX_UNUSED_GPIO 35
