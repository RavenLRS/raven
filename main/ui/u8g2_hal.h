#pragma once

typedef struct u8g2_struct u8g2_t;
typedef struct screen_i2c_config_s screen_i2c_config_t;

void u8g2_init_ssd1306_128x64_noname(screen_i2c_config_t *cfg, u8g2_t *u8g2);