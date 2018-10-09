#include <hal/log.h>

#include <os/os.h>

#include "target.h"

#ifdef USE_SCREEN

#include <u8g2.h>

#include "ui/screen_i2c.h"
#include "ui/u8g2_hal.h"

#define ACK_CHECK_EN true

typedef struct u8g2_hal_s
{
    hal_i2c_bus_t i2c_bus;
    hal_gpio_t rst;
} u8g2_hal_t;

static u8g2_hal_t hal;

static uint8_t u8g2_msg_i2c_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{

    switch (msg)
    {
    case U8X8_MSG_BYTE_SET_DC:
        break;

    case U8X8_MSG_BYTE_INIT:
        // We already initialize the driver in screen_i2c.c
        // to detect if the screen is present.
        break;

    case U8X8_MSG_BYTE_SEND:
    {
        uint8_t cmddata;
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        ESP_ERROR_CHECK(i2c_master_start(cmd));
        ESP_ERROR_CHECK(i2c_master_write_byte(cmd, (u8x8_GetI2CAddress(u8x8) << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN));
        if (arg_int == 1)
        {
            cmddata = 0;
            ESP_ERROR_CHECK(i2c_master_write(cmd, &cmddata, 1, ACK_CHECK_EN));
        }
        else
        {
            cmddata = 0x40;
            ESP_ERROR_CHECK(i2c_master_write(cmd, &cmddata, 1, ACK_CHECK_EN));
            // bzero(arg_ptr,arg_int);
            // *data=0x40;
        }
        ESP_ERROR_CHECK(i2c_master_write(cmd, arg_ptr, arg_int, ACK_CHECK_EN));
        /*
		    while( arg_int > 0 ) {
			   ESP_ERROR_CHECK(i2c_master_write_byte(cmd, *data, ACK_CHECK_EN));
//			   printf("0x%02X ",*data);
			   data++;
			   arg_int--;
		    }
//			printf("\n");
*/
        ESP_ERROR_CHECK(i2c_master_stop(cmd));
        ESP_ERROR_CHECK(i2c_master_cmd_begin(hal.i2c_bus, cmd, portMAX_DELAY));
        i2c_cmd_link_delete(cmd);
        break;
    }
    }
    return 0;
}

/*
 * HAL callback function as prescribed by the U8G2 library.  This callback is invoked
 * to handle callbacks for I²C and delay functions.
 */
static uint8_t u8g2_msg_i2c_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    switch (msg)
    {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        if (hal.rst != HAL_GPIO_NONE)
        {
            HAL_ERR_ASSERT_OK(hal_gpio_setup(hal.rst, HAL_GPIO_DIR_OUTPUT, HAL_GPIO_PULL_DOWN));
        }
        break;
        // Set the GPIO reset pin to the value passed in through arg_int.
    case U8X8_MSG_GPIO_RESET:
        if (hal.rst != HAL_GPIO_NONE)
        {
            HAL_ERR_ASSERT_OK(hal_gpio_set_level(hal.rst, arg_int));
        }
        break;
        // Set the GPIO client select pin to the value passed in through arg_int.
    case U8X8_MSG_GPIO_CS:
        break;
        // Set the Software I²C pin to the value passed in through arg_int.
    case U8X8_MSG_GPIO_I2C_CLOCK:
        break;
        // Set the Software I²C pin to the value passed in through arg_int.
    case U8X8_MSG_GPIO_I2C_DATA:
        break;
        // Delay for the number of milliseconds passed in through arg_int.
    case U8X8_MSG_DELAY_MILLI:
        //vTaskDelay(arg_int / portTICK_PERIOD_MS);
        break;
    }
    return 0;
}

void u8g2_init_ssd1306_128x64_noname(screen_i2c_config_t *cfg, u8g2_t *u8g2)
{
    hal.i2c_bus = cfg->i2c_bus;
    hal.rst = cfg->rst;

    u8g2_Setup_ssd1306_128x64_noname_f(
        u8g2,
        U8G2_R0,
        //u8x8_byte_sw_i2c,
        u8g2_msg_i2c_cb,
        u8g2_msg_i2c_and_delay_cb);
    u8x8_SetI2CAddress(&u8g2->u8x8, cfg->addr);
}

#endif