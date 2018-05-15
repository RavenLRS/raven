#pragma once

// User button
#define PIN_BUTTON_1 0

// LoRa connection
// GPIO5  -- SX1278's SCK
// GPIO19 -- SX1278's MISO
// GPIO27 -- SX1278's MOSI
// GPIO18 -- SX1278's CS
// GPIO14 -- SX1278's RESET
// GPIO26 -- SX1278's DIO0

#define PIN_LORA_SCK 5
#define PIN_LORA_MISO 19
#define PIN_LORA_MOSI 27
#define PIN_LORA_CS 18
#define PIN_LORA_RST 14
#define PIN_LORA_DIO0 26

// 0 is wired to button
// 2 needs to be left unconnected for flashing
// 4 is OLED SDA (boards with OLED)
// 5 is LoRa SCK
// 12 can't be pulled low during boot, otherwise we
// boot in VCC = 1.8V mode. See https://github.com/espressif/esptool/wiki/ESP32-Boot-Mode-Selection
// 14 is LoRa RST
// 15 is OLED SCL (boards with OLED)
// 16 is OLED RST (boards with OLED)
// 18 is LoRa CS
// 19 is LoRa MOSI
// 25 is LED
// 26 is LoRa IRQ
// 27 is LoRa MOSI
// 34..39 is input only
#define PIN_USABLE_BASE_MASK (PIN_N(1) | PIN_N(3) | PIN_N(4) | PIN_N(13) | PIN_N(15) | PIN_N(16) | PIN_N(17) | PIN_N(21) | PIN_N(22) | PIN_N(23) | PIN_N(32) | PIN_N(33))

#if !defined(PIN_SCREEN_SDA) || !defined(PIN_SCREEN_SCL) || !defined(PIN_SCREEN_RST) || !defined(SCREEN_I2C_ADDR)
#define PIN_USABLE_MASK PIN_USABLE_BASE_MASK
#else
#define PIN_USABLE_MASK (PIN_USABLE_BASE_MASK & ~(PIN_N(PIN_SCREEN_SDA) | PIN_N(PIN_SCREEN_SCL) | PIN_N(PIN_SCREEN_RST)))
#endif

#define PIN_DEFAULT_TX 13
#define PIN_DEFAULT_RX 21

#define PIN_UNUSED_TX 2
#define PIN_UNUSED_RX 35
