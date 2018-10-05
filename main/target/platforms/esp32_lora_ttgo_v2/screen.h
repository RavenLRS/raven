#define USE_SCREEN

#define SCREEN_I2C_BUS I2C_NUM_1
#define SCREEN_GPIO_SDA 21
#define SCREEN_GPIO_SCL 22
// We don't use the OLED RST pin because it causes reboots on some boards
// (probably due to some incorrect wiring)
#define SCREEN_GPIO_RST HAL_GPIO_NONE
#define SCREEN_I2C_ADDR 0x3c
