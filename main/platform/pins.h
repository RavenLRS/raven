#pragma once

#include <driver/gpio.h>

#define PIN_USABLE_COUNT 8

#define PIN_NUM_13_IDX 2
#define PIN_NUM_21_IDX 4

#define PIN_DEFAULT_TX GPIO_NUM_13
#define PIN_DEFAULT_RX GPIO_NUM_21
#define PIN_DEFAULT_TX_IDX PIN_NUM_13_IDX
#define PIN_DEFAULT_RX_IDX PIN_NUM_21_IDX

// We need to remap unused serial pins because some of
// the default ones are used by other peripherals.
// e.g. 16 is U2_RXD but is connected to OLED RST.
#define UNUSED_TX_PIN GPIO_NUM_33
#define UNUSED_RX_PIN GPIO_NUM_35

gpio_num_t usable_pin_at(int idx);
