/*
 * platform.h
 *
 *  Created on: Jul 23, 2018
 *      Author: hdphilip
 */

#include "platform/platform_macros.h"

#include "target/bands/433.h"

#define PIN_BUTTON_1 0

#define PIN_BEEPER 25  // This is Led D2 (red) on the microRX
#define PIN_LED_1 4    // This is Led D1 (Green)

#define USE_SCREEN
#define PIN_SCREEN_SDA 21
#define PIN_SCREEN_SCL 22
// We don't use the OLED RST pin because it causes reboots on some boards
// (probably due to some incorrect wiring)
#define PIN_SCREEN_RST 0
#define SCREEN_I2C_ADDR 0x3c


#define PIN_SX127X_SCK 5
#define PIN_SX127X_MISO 19
#define PIN_SX127X_MOSI 27
#define PIN_SX127X_CS 18
#define PIN_SX127X_RST 14
#define PIN_SX127X_DIO0 26
#define SX127X_OUTPUT_TYPE SX127X_OUTPUT_PA_BOOST
#define PIN_DEFAULT_TX 23
#define PIN_DEFAULT_RX 23

#define PIN_UNUSED_TX 2
#define PIN_UNUSED_RX 35

#define PIN_USABLE_MASK (PIN_N(PIN_DEFAULT_TX) | PIN_N(PIN_DEFAULT_RX))
#define BOARD_NAME "ESP32+LoRa MicroRX v1"

