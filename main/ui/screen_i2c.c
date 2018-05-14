#include <driver/gpio.h>
#include <driver/i2c.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "platform.h"

#ifdef USE_SCREEN

#include <stdio.h>
#include <string.h>
#include <u8g2.h>

#include "u8g2_esp32_hal.h"

#include "screen_i2c.h"

#define SCREEN_I2C_MASTER_NUM I2C_NUM_1
// this is the max frequency allowed by the OLED screen
#define SCREEN_I2C_MASTER_FREQ_HZ 1800000

static const char *TAG = "screen.i2c";

static void screen_i2c_install_driver(screen_i2c_config_t *cfg)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = cfg->sda;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = cfg->scl;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = SCREEN_I2C_MASTER_FREQ_HZ;
    ESP_ERROR_CHECK(i2c_param_config(SCREEN_I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(SCREEN_I2C_MASTER_NUM, conf.mode, 0, 0, 0));
}

bool screen_i2c_init(screen_i2c_config_t *cfg, u8g2_t *u8g2)
{
    gpio_set_direction(cfg->rst, GPIO_MODE_OUTPUT);
    gpio_set_level(cfg->rst, 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(cfg->rst, 1);

    screen_i2c_install_driver(cfg);

    // Detect if the screen is present

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    ESP_ERROR_CHECK(i2c_master_start(cmd));
    ESP_ERROR_CHECK(i2c_master_write_byte(cmd, (cfg->addr << 1) | I2C_MASTER_WRITE, true));
    ESP_ERROR_CHECK(i2c_master_stop(cmd));
    esp_err_t ack_err = (i2c_master_cmd_begin(SCREEN_I2C_MASTER_NUM, cmd, 10 / portTICK_PERIOD_MS));
    i2c_cmd_link_delete(cmd);

    if (ack_err != ESP_OK)
    {
        gpio_set_level(cfg->rst, 0);
        ESP_ERROR_CHECK(i2c_driver_delete(SCREEN_I2C_MASTER_NUM));
        return false;
    }

    u8g2_esp32_hal_t hal = {
        .i2c_port = SCREEN_I2C_MASTER_NUM,
        .reset = cfg->rst,
    };

    u8g2_esp32_hal_init(hal);

    u8g2_Setup_ssd1306_128x64_noname_f(
        u8g2,
        U8G2_R0,
        //u8x8_byte_sw_i2c,
        u8g2_esp32_msg_i2c_cb,
        u8g2_esp32_msg_i2c_and_delay_cb); // init u8g2 structure
    u8x8_SetI2CAddress(&u8g2->u8x8, cfg->addr);

    screen_i2c_power_on(cfg, u8g2);
    return true;
}

void screen_i2c_shutdown(screen_i2c_config_t *cfg)
{
    gpio_set_level(cfg->rst, 0);
}

void screen_i2c_power_on(screen_i2c_config_t *cfg, u8g2_t *u8g2)
{
    gpio_set_level(cfg->rst, 1);

    ESP_LOGI(TAG, "u8g2_InitDisplay");
    u8g2_InitDisplay(u8g2); // send init sequence to the display, display is in sleep mode after this,

    ESP_LOGI(TAG, "u8g2_SetPowerSave");
    u8g2_SetPowerSave(u8g2, 0); // wake up display
    // This reduces the contrast to the minimum to save power
    u8g2_SetContrast(u8g2, 0);
    u8g2_ClearBuffer(u8g2);
}

#endif