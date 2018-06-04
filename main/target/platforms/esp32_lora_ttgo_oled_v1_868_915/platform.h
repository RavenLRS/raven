#include "target/bands/868_915.h"

#include "target/platforms/esp32_lora_ttgo_v1/common.h"
#include "target/platforms/esp32_lora_ttgo_v1/screen.h"

#define PIN_LED_1 25
#define PIN_USABLE_MASK (PIN_USABLE_BASE_MASK & ~(PIN_N(PIN_SCREEN_SDA) | PIN_N(PIN_SCREEN_SCL) | PIN_N(PIN_SCREEN_RST)))
#define BOARD_NAME "ESP32+LoRa+OLED TTGO 868/915MHz"