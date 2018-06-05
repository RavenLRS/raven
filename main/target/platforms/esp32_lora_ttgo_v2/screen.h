#define USE_SCREEN

#define PIN_SCREEN_SDA 21
#define PIN_SCREEN_SCL 22
// We don't use the OLED RST pin because it causes reboots on some boards
// (probably due to some incorrect wiring)
#define PIN_SCREEN_RST 0
#define SCREEN_I2C_ADDR 0x3c
