#include <hal/gpio.h>

#include "target/bands/433.h"
#include "target/platforms/esp32/single_button.h"

// Buzzer
#define USE_BEEPER
#define BEEPER_GPIO 12

//BUTTON
#define LED_1_GPIO 16

//SCREEN
#define USE_SCREEN
#define SCREEN_I2C_BUS I2C_NUM_1
#define SCREEN_I2C_ADDR 0x3c

#define SCREEN_GPIO_SDA 22
#define SCREEN_GPIO_SCL 23
// We don't use the OLED RST pin because it causes reboots on some boards
// (probably due to some incorrect wiring)
#define SCREEN_GPIO_RST 0

// SX127X
#define USE_RADIO_SX127X
#define SX127X_SPI_BUS VSPI_HOST
#define SX127X_GPIO_SCK 25
#define SX127X_GPIO_MISO 27
#define SX127X_GPIO_MOSI 26
#define SX127X_GPIO_CS 33
#define SX127X_GPIO_RST 32
#define SX127X_GPIO_DIO0 18

#define SX127X_OUTPUT_TYPE SX127X_OUTPUT_PA_BOOST

#define TX_DEFAULT_GPIO 14
#define RX_DEFAULT_GPIO 13

#define TX_UNUSED_GPIO 2
#define RX_UNUSED_GPIO 35

#define HAL_GPIO_USER_MASK (HAL_GPIO_M(TX_DEFAULT_GPIO) | HAL_GPIO_M(RX_DEFAULT_GPIO) | HAL_GPIO_M(TX_UNUSED_GPIO) | HAL_GPIO_M(RX_UNUSED_GPIO))

#define BOARD_NAME "AFnano_433"
