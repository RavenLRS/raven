// ESP32 with a single button should always use
// GPIO0, since user must be able to tie GPIO0
// to ground in order to get into the bootloader
// and flash the firmware.
#define USE_BUTTON_SINGLE
#define BUTTON_ENTER_GPIO 0