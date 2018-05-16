#pragma once

#include <stdbool.h>

#include "rc/rc_data.h"

extern const char *pwm_channel_names[];

typedef enum
{
    PWM_CHANNEL_NONE,
    PWM_CHANNEL_1,
    PWM_CHANNEL_2,
    PWM_CHANNEL_3,
    PWM_CHANNEL_4,
    PWM_CHANNEL_5,
    PWM_CHANNEL_6,
    PWM_CHANNEL_7,
    PWM_CHANNEL_8,
    PWM_CHANNEL_9,
    PWM_CHANNEL_10,
    PWM_CHANNEL_11,
    PWM_CHANNEL_12,
#if RC_CHANNELS_NUM > 12
    PWM_CHANNEL_13,
    PWM_CHANNEL_14,
    PWM_CHANNEL_15,
    PWM_CHANNEL_16,
#if RC_CHANNELS_NUM > 16
    PWM_CHANNEL_17,
    PWM_CHANNEL_18,
#if RC_CHANNELS_NUM > 18
    PWM_CHANNEL_19,
    PWM_CHANNEL_20,
#endif
#endif
#endif
    PWM_CHANNEL_COUNT,
} pwm_channel_e;

_Static_assert(RC_CHANNELS_NUM == 12 || RC_CHANNELS_NUM == 16 || RC_CHANNELS_NUM == 18 || RC_CHANNELS_NUM == 20, "Adjust pwm_channel_e to support RC_CHANNELS_NUM");

void pwm_init(void);
void pwm_update_config(void);
void pwm_update(const rc_data_t *rc_data);
bool pwm_output_can_use_pin(int pin);
