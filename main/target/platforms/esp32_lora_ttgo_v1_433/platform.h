#include "target/bands/433.h"

#include "target/platforms/esp32_lora_ttgo_v1/common.h"

#define LED_1_GPIO 2
#define LED_1_USE_PWM
#define HAL_GPIO_USER_MASK HAL_GPIO_USER_BASE_MASK
#define BOARD_NAME "ESP32+LoRa TTGO 433MHz"
