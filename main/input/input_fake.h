#pragma once

#include "input/input.h"

#include "rc/rc_data.h"

typedef struct input_fake_s
{
    input_t input;
    bool channels[RC_CHANNELS_NUM];
    time_micros_t next_flip;
    time_micros_t next_update;
} input_fake_t;

void input_fake_init(input_fake_t *input);