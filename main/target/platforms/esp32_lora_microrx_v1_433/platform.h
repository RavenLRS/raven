/*
 * platform.h
 *
 *  Created on: Jul 23, 2018
 *      Author: hdphilip
 */

#include "target/bands/433.h"

#define BUTTON_1_GPIO 0

#define BEEPER_GPIO 25 // This is Led D2 (red) on the microRX
#define LED_1_GPIO 4   // This is Led D1 (Green)
#define LED_1_USE_PWM

#define USE_SCREEN
#define SCREEN_GPIO_SDA 21
#define SCREEN_GPIO_SCL 22
// We don't use the OLED RST pin because it causes reboots on some boards
// (probably due to some incorrect wiring)
#define SCREEN_GPIO_RST 0
#define SCREEN_I2C_ADDR 0x3c

#define SX127X_GPIO_SCK 5
#define SX127X_GPIO_MISO 19
#define SX127X_GPIO_MOSI 27
#define SX127X_GPIO_CS 18
#define SX127X_GPIO_RST 14
#define SX127X_GPIO_DIO0 26
#define SX127X_OUTPUT_TYPE SX127X_OUTPUT_PA_BOOST
#define TX_DEFAULT_GPIO 23
#define RX_DEFAULT_GPIO 23

#define TX_UNUSED_GPIO 2
#define RX_UNUSED_GPIO 35

#define HAL_GPIO_USER_MASK (HAL_GPIO_M(TX_DEFAULT_GPIO) | HAL_GPIO_M(RX_DEFAULT_GPIO))
#define BOARD_NAME "ESP32+LoRa MicroRX v1"
