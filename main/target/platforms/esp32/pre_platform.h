#pragma once

// All ESP32 targets support these features
#define USE_P2P
#define USE_BLUETOOTH
#define USE_OTA
#define USE_DEVELOPER_MENU
#define USE_IDF_WMONITOR

#define RC_TASK_STACK_SIZE 4096 // We need a bigger stack on ESP32 because of the SPI libraries
