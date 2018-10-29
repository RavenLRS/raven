#include "target/bands/868_915.h"

#include "target/platforms/esp32_lora_ttgo_v1/common.h"
#include "target/platforms/esp32_lora_ttgo_v1/screen.h"

#define LED_1_GPIO 25
#define LED_1_USE_PWM
#define HAL_GPIO_USER_MASK (HAL_GPIO_USER_BASE_MASK & ~(HAL_GPIO_M(SCREEN_GPIO_SDA) | HAL_GPIO_M(SCREEN_GPIO_SCL) | HAL_GPIO_M(SCREEN_GPIO_RST)))
#define BOARD_NAME "ESP32+LoRa+OLED TTGO 868/915MHz"
