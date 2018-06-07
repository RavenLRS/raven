#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct u8g2_struct u8g2_t;

typedef struct screen_i2c_config_s
{
    uint8_t sda;
    uint8_t scl;
    uint8_t rst;
    uint8_t addr;
} screen_i2c_config_t;

bool screen_i2c_init(screen_i2c_config_t *cfg, u8g2_t *u8g2);
void screen_i2c_shutdown(screen_i2c_config_t *cfg, u8g2_t *u8g2);
void screen_i2c_power_on(screen_i2c_config_t *cfg, u8g2_t *u8g2);
