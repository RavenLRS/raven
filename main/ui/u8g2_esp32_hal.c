#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <esp_log.h>

#include "target.h"

#ifdef USE_SCREEN

#include "u8g2_esp32_hal.h"

#define ACK_CHECK_EN true

static const char *TAG = "u8g2_hal";

static u8g2_esp32_hal_t u8g2_esp32_hal; // HAL state data.

/*
 * Initialze the ESP32 HAL.
 */
void u8g2_esp32_hal_init(u8g2_esp32_hal_t u8g2_esp32_hal_param)
{
    u8g2_esp32_hal = u8g2_esp32_hal_param;
} // u8g2_esp32_hal_init

/*
 * HAL callback function as prescribed by the U8G2 library.  This callback is invoked
 * to handle callbacks for communications.
 */
uint8_t u8g2_esp32_msg_i2c_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{

    //	ESP_LOGD(TAG, "msg_i2c_cb: Received a msg: %d %d", msg, arg_int);
    //ESP_LOGD(tag, "msg_i2c_cb: Received a msg: %d: %s", msg, msgToString(msg, arg_int, arg_ptr));

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
        uint8_t *data;
        uint8_t cmddata;
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        ESP_ERROR_CHECK(i2c_master_start(cmd));
        //			ESP_LOGI(TAG, "I2CAddress %02X", u8x8_GetI2CAddress(u8x8)>>1);
        ESP_ERROR_CHECK(i2c_master_write_byte(cmd, (u8x8_GetI2CAddress(u8x8) << 1) | I2C_MASTER_WRITE, ACK_CHECK_EN));
        data = (uint8_t *)arg_ptr;
        if (arg_int == 1)
        {
            cmddata = 0;
            ESP_ERROR_CHECK(i2c_master_write(cmd, &cmddata, 1, ACK_CHECK_EN));
            //				printf("0x%02X ",cmddata);
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
        ESP_ERROR_CHECK(i2c_master_cmd_begin(u8g2_esp32_hal.i2c_port, cmd, portMAX_DELAY));
        i2c_cmd_link_delete(cmd);
        break;
    }
    }
    return 0;
} // u8g2_esp32_msg_i2c_cb

/*
 * HAL callback function as prescribed by the U8G2 library.  This callback is invoked
 * to handle callbacks for GPIO and delay functions.
 */
uint8_t u8g2_esp32_msg_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    //ESP_LOGD(tag, "msg_gpio_and_delay_cb: Received a msg: %d: %s", msg, msgToString(msg, arg_int, arg_ptr));
    switch (msg)
    {

        // Initialize the GPIO and DELAY HAL functions.  If the pins for DC and RESET have been
        // specified then we define those pins as GPIO outputs.
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
    {
        uint64_t bitmask = 0;
        if (u8g2_esp32_hal.reset != U8G2_ESP32_HAL_UNDEFINED)
        {
            bitmask = bitmask | (1 << u8g2_esp32_hal.reset);
        }

        gpio_config_t gpioConfig;
        gpioConfig.pin_bit_mask = bitmask;
        gpioConfig.mode = GPIO_MODE_OUTPUT;
        gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
        gpioConfig.pull_down_en = GPIO_PULLDOWN_ENABLE;
        gpioConfig.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&gpioConfig);
        break;
    }

        // Set the GPIO reset pin to the value passed in through arg_int.
    case U8X8_MSG_GPIO_RESET:
        if (u8g2_esp32_hal.reset != U8G2_ESP32_HAL_UNDEFINED)
        {
            gpio_set_level(u8g2_esp32_hal.reset, arg_int);
        }
        break;

        // Delay for the number of milliseconds passed in through arg_int.
    case U8X8_MSG_DELAY_MILLI:
        vTaskDelay(arg_int / portTICK_PERIOD_MS);
        break;
    }
    return 0;
} // u8g2_esp32_msg_gpio_and_delay_cb

/*
 * HAL callback function as prescribed by the U8G2 library.  This callback is invoked
 * to handle callbacks for I²C and delay functions.
 */
uint8_t u8g2_esp32_msg_i2c_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{

    ESP_LOGD(TAG, "msg_i2c_and_delay_cb: Received a msg: %d", msg);

    switch (msg)
    {
        // Initialize the GPIO and DELAY HAL functions.  If the pins for DC and RESET have been
        // specified then we define those pins as GPIO outputs.
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
    {
        uint64_t bitmask = 0;
        if (u8g2_esp32_hal.reset != U8G2_ESP32_HAL_UNDEFINED)
        {
            bitmask = bitmask | (1 << u8g2_esp32_hal.reset);
        }
        if (bitmask == 0)
        {
            break;
        }
        gpio_config_t gpioConfig;
        gpioConfig.pin_bit_mask = bitmask;
        gpioConfig.mode = GPIO_MODE_OUTPUT;
        gpioConfig.pull_up_en = GPIO_PULLUP_DISABLE;
        gpioConfig.pull_down_en = GPIO_PULLDOWN_ENABLE;
        gpioConfig.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&gpioConfig);
        break;
    }

        // Set the GPIO reset pin to the value passed in through arg_int.
    case U8X8_MSG_GPIO_RESET:
        if (u8g2_esp32_hal.reset != U8G2_ESP32_HAL_UNDEFINED)
        {
            gpio_set_level(u8g2_esp32_hal.reset, arg_int);
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
} // u8g2_esp32_msg_gpio_and_delay_cb

#endif