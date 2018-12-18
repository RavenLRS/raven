#pragma once

#include <hal/gpio.h>
#include "input/input.h"
#include "rc/rc_data.h"

#define PPM_CAPTURE_COUNT           12
#define PPM_IN_MIN_SYNC_PULSE_US    2700    // microseconds
#define PPM_IN_MIN_CHANNEL_PULSE_US 750     // microseconds
#define PPM_IN_MAX_CHANNEL_PULSE_US 2250    // microseconds
#define PPM_STABLE_FRAMES_REQUIRED_COUNT    25
#define PPM_IN_MIN_NUM_CHANNELS     4
#define PPM_IN_MAX_NUM_CHANNELS     PPM_CAPTURE_COUNT
#define PPM_RCVR_TIMEOUT            0
#define PPM_IN_MIN_CHANNEL_VALUE  1000
#define PPM_IN_MAX_CHANNEL_VALUE  2000
#define PPM_PULSE_QUEUE_SIZE      8

typedef struct input_ppm_config_s
{
    hal_gpio_t gpio;
} input_ppm_config_t;

typedef struct input_ppm_s
{
    input_t input;
    hal_gpio_t gpio;
    uint32_t captures[RC_CHANNELS_NUM];
    time_micros_t last_pulse;
    time_micros_t pulse_queue[PPM_PULSE_QUEUE_SIZE];
    int      pulseCountInQueue;
    bool     tracking;
    uint8_t  pulseIndex;
    int8_t   numChannels;
    int8_t   numChannelsPrevFrame;
    uint8_t  stableFramesSeenCount;
} input_ppm_t;

void input_ppm_init(input_ppm_t *input);