/*
 * platform.h
 *
 *  Created on: Jul 23, 2018
 *      Author: hdphilip
 */

#include "target/bands/433.h"
#include "target/platforms/esp32_lora_ttgo_v1/common.h"
#include "target/platforms/esp32_lora_ttgo_v1/screen.h"
#define LED_1_GPIO 25 // Only difference with TTGO is LED pin
#define LED_1_USE_PWM
#define HAL_GPIO_USER_MASK (HAL_GPIO_USER_BASE_MASK & ~(HAL_GPIO_M(SCREEN_GPIO_SDA) | HAL_GPIO_M(SCREEN_GPIO_SCL) | HAL_GPIO_M(SCREEN_GPIO_RST)))
#define BOARD_NAME "ESP32 high power v1"
