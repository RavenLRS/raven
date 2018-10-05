#include "target.h"

#ifdef USE_SCREEN

#include <hal/gpio.h>
#include <hal/i2c.h>
#include <hal/log.h>

#include <os/os.h>

#include <stdio.h>
#include <string.h>

#include <u8g2.h>

#include "ui/u8g2_hal.h"

#include "screen_i2c.h"

// this is the max frequency allowed by the OLED screen
#define SCREEN_I2C_MASTER_FREQ_HZ 1800000

static const char *TAG = "screen.i2c";

bool screen_i2c_init(screen_i2c_config_t *cfg, u8g2_t *u8g2)
{
    if (cfg->rst != HAL_GPIO_NONE)
    {
        HAL_ERR_ASSERT_OK(hal_gpio_enable(cfg->rst));
        HAL_ERR_ASSERT_OK(hal_gpio_set_dir(cfg->rst, HAL_GPIO_DIR_OUTPUT));
        HAL_ERR_ASSERT_OK(hal_gpio_set_level(cfg->rst, HAL_GPIO_LOW));
        vTaskDelay(50 / portTICK_PERIOD_MS);
        HAL_ERR_ASSERT_OK(hal_gpio_set_level(cfg->rst, HAL_GPIO_HIGH));
    }

    HAL_ERR_ASSERT_OK(hal_i2c_bus_init(cfg->i2c_bus, cfg->sda, cfg->scl, SCREEN_I2C_MASTER_FREQ_HZ));

    // Detect if the screen is present
    hal_i2c_cmd_t cmd;
    HAL_ERR_ASSERT_OK(hal_i2c_cmd_init(&cmd));

    HAL_ERR_ASSERT_OK(hal_i2c_cmd_master_start(&cmd));
    HAL_ERR_ASSERT_OK(hal_i2c_cmd_master_write_byte(&cmd, HAL_I2C_WRITE_ADDR(cfg->addr), true));
    HAL_ERR_ASSERT_OK(hal_i2c_cmd_master_stop(&cmd));

    hal_err_t err = hal_i2c_cmd_master_exec(cfg->i2c_bus, &cmd);
    HAL_ERR_ASSERT_OK(hal_i2c_cmd_destroy(&cmd));

    if (err != HAL_ERR_NONE)
    {
        if (cfg->rst != HAL_GPIO_NONE)
        {
            hal_gpio_set_level(cfg->rst, HAL_GPIO_LOW);
        }
        HAL_ERR_ASSERT_OK(hal_i2c_bus_deinit(cfg->i2c_bus));
        return false;
    }

    u8g2_init_ssd1306_128x64_noname(cfg, u8g2);

    screen_i2c_power_on(cfg, u8g2);
    return true;
}

void screen_i2c_shutdown(screen_i2c_config_t *cfg, u8g2_t *u8g2)
{
    LOG_I(TAG, "Screen shutdown");
    if (cfg->rst != HAL_GPIO_NONE)
    {
        hal_gpio_set_level(cfg->rst, HAL_GPIO_LOW);
    }
    else
    {
        // No RST pin, just turn on powersave mode
        u8g2_SetPowerSave(u8g2, 1);
    }
    return false;
}

void screen_i2c_power_on(screen_i2c_config_t *cfg, u8g2_t *u8g2)
{
    LOG_I(TAG, "Screen power on");
    if (cfg->rst != HAL_GPIO_NONE)
    {
        hal_gpio_set_level(cfg->rst, HAL_GPIO_HIGH);
    }

    LOG_D(TAG, "u8g2_InitDisplay");
    u8g2_InitDisplay(u8g2); // send init sequence to the display, display is in sleep mode after this,

    LOG_D(TAG, "u8g2_SetPowerSave");
    u8g2_SetPowerSave(u8g2, 0); // wake up display
    // This reduces the contrast to the minimum to save power
    u8g2_SetContrast(u8g2, 0);
    u8g2_ClearBuffer(u8g2);
}

#endif