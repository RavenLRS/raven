#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <hal/err.h>
#include <hal/gpio.h>

#define HAL_I2C_WRITE_FLAG 0
#define HAL_I2C_READ_FLAG 1

#define HAL_I2C_WRITE_ADDR(addr) ((addr << 1) | HAL_I2C_WRITE_FLAG)
#define HAL_I2C_READ_ADDR(addr) ((addr << 1) | HAL_I2C_READ_FLAG)

typedef struct hal_i2c_cmd_s hal_i2c_cmd_t;

hal_err_t hal_i2c_bus_init(hal_i2c_bus_t bus, hal_gpio_t sda, hal_gpio_t scl, uint32_t freq_hz);
hal_err_t hal_i2c_bus_deinit(hal_i2c_bus_t bus);

hal_err_t hal_i2c_cmd_init(hal_i2c_cmd_t *cmd);
hal_err_t hal_i2c_cmd_destroy(hal_i2c_cmd_t *cmd);

hal_err_t hal_i2c_cmd_master_start(hal_i2c_cmd_t *cmd);
hal_err_t hal_i2c_cmd_master_stop(hal_i2c_cmd_t *cmd);

hal_err_t hal_i2c_cmd_master_write_byte(hal_i2c_cmd_t *cmd, uint8_t data, bool ack_en);

hal_err_t hal_i2c_cmd_master_exec(hal_i2c_bus_t bus, hal_i2c_cmd_t *cmd);