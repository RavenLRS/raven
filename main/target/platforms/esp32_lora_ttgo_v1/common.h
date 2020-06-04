#include "target/platforms/esp32/single_button.h"

// Buzzer
#define USE_BEEPER
#define BEEPER_GPIO 12 // This seems appropriate for TTGO v1 boards, since 12 can't be pulled low during boot and it's not connected to anything

#define USE_RADIO_SX127X

// SX127X connection
// GPIO5  -- SX1278's SCK
// GPIO19 -- SX1278's MISO
// GPIO27 -- SX1278's MOSI
// GPIO18 -- SX1278's CS
// GPIO14 -- SX1278's RESET
// GPIO26 -- SX1278's DIO0

#define SX127X_SPI_BUS VSPI_HOST
#define SX127X_GPIO_SCK 5
#define SX127X_GPIO_MISO 19
#define SX127X_GPIO_MOSI 27
#define SX127X_GPIO_CS 18
#define SX127X_GPIO_RST 14
#define SX127X_GPIO_DIO0 26

// All v1 boards use PA_BOOST
#define SX127X_OUTPUT_TYPE SX127X_OUTPUT_PA_BOOST

// 0 is wired to button
// 2 needs to be left unconnected for flashing
// 4 is OLED SDA (boards with OLED)
// 5 is SX127X SCK
// 12 can't be pulled low during boot, otherwise we
// boot in VCC = 1.8V mode. See https://github.com/espressif/esptool/wiki/ESP32-Boot-Mode-Selection
// 14 is SX127X RST
// 15 is OLED SCL (boards with OLED)
// 16 is OLED RST (boards with OLED)
// 18 is SX127X CS
// 19 is SX127X MOSI
// 25 is LED (in some boards)
// 26 is SX127X IRQ
// 27 is SX127X MOSI
// 34..39 is input only
#define HAL_GPIO_USER_BASE_MASK (HAL_GPIO_M(1) | HAL_GPIO_M(3) | HAL_GPIO_M(4) | HAL_GPIO_M(13) | HAL_GPIO_M(15) | HAL_GPIO_M(16) | HAL_GPIO_M(17) | HAL_GPIO_M(21) | HAL_GPIO_M(22) | HAL_GPIO_M(23) | HAL_GPIO_M(32) | HAL_GPIO_M(33))

#define TX_DEFAULT_GPIO 13
#define RX_DEFAULT_GPIO 21

#define TX_UNUSED_GPIO 2
#define RX_UNUSED_GPIO 35
