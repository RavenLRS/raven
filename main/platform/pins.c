#include "util/macros.h"

#include "pins.h"

// XXX: Keep this in sync with index constants in pins.h
static const gpio_num_t usable_pins[] = {
    // 0 is wired to button
    GPIO_NUM_1,
    // 2 needs to be left unconnected for flashing
    // GPIO_NUM_2,
    GPIO_NUM_3,
    // 4 is OLED SDA
    // 5 is LoRa SCK
    // 12 can't be pulled low during boot, otherwise we
    // boot in VCC = 1.8V mode. See https://github.com/espressif/esptool/wiki/ESP32-Boot-Mode-Selection
    // GPIO_NUM_12,
    GPIO_NUM_13,
    // 14 is LoRa RST
    // 16 is OLED RST
    GPIO_NUM_17,
    // 18 is LoRa CS
    // 19 is LoRa MOSI
    GPIO_NUM_21,
    GPIO_NUM_22,
    GPIO_NUM_23,
    // 25 is LED
    // 26 is LoRa IRQ
    // 27 is LoRa MOSI
    GPIO_NUM_32,
    //GPIO_NUM_33, // 33 is used an unused TX pin
    // 34..39 is input only
};

ARRAY_ASSERT_COUNT(usable_pins, PIN_USABLE_COUNT, "count(usable_pins) != PIN_USABLE_COUNT");

gpio_num_t usable_pin_at(int idx) { return usable_pins[idx]; }