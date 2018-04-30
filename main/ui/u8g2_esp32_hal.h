/*
 * u8g2_esp32_hal.h
 *
 *  Created on: Feb 12, 2017
 *      Author: kolban
 */

#ifndef U8G2_ESP32_HAL_H_
#define U8G2_ESP32_HAL_H_

#include "u8g2.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"

#define U8G2_ESP32_HAL_UNDEFINED (-1)

typedef struct
{
	i2c_port_t i2c_port;
	gpio_num_t reset;
} u8g2_esp32_hal_t;

#define U8G2_ESP32_HAL_DEFAULT                             \
	{                                                      \
		U8G2_ESP32_HAL_UNDEFINED, U8G2_ESP32_HAL_UNDEFINED \
	}

void u8g2_esp32_hal_init(u8g2_esp32_hal_t u8g2_esp32_hal_param);
uint8_t u8g2_esp32_msg_comms_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8g2_esp32_msg_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8g2_esp32_msg_i2c_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8g2_esp32_msg_i2c_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
#endif /* U8G2_ESP32_HAL_H_ */
