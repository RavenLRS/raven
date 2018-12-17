
#include "input_ppm.h"
#include "rc/rc_data.h"
#include "util/time.h"
#include <hal/gpio.h>
#include <hal/rand.h>

#define PPM_VALUE_MAPPING(ch) RC_CHANNEL_MIN_VALUE + (ch - PPM_IN_MIN_CHANNEL_PULSE_US) * (RC_CHANNEL_MAX_VALUE - RC_CHANNEL_MIN_VALUE) / (PPM_IN_MAX_CHANNEL_PULSE_US - PPM_IN_MIN_CHANNEL_PULSE_US)

static bool input_ppm_open(void *input, void *config)
{
    input_ppm_t *input_ppm = input;
    HAL_ERR_ASSERT_OK(hal_gpio_setup(input_ppm->gpio, HAL_GPIO_DIR_INPUT, HAL_GPIO_PULL_DOWN));

    //HAL_ERR_ASSERT_OK(hal_gpio_set_isr(input->gpio, GPIO_INTR_POSEDGE , lora_handle_isr, sx127x));
    time_micros_t now = time_micros_now();
    failsafe_set_max_interval(&input_ppm->input.failsafe, 1000);
    failsafe_reset_interval(&input_ppm->input.failsafe, now);

    input_ppm->last_gpio_level = hal_gpio_get_level(input_ppm->gpio);

    input_ppm->last_pulse = 0;
    INPUT_SET_MSP_TRANSPORT(input_ppm, NULL);
    return true;
}

static bool input_ppm_update(void *input, rc_data_t *data, time_micros_t now)
{
    input_ppm_t *input_ppm = input;
    int32_t i;
    bool updated = false;

    int gpio_level = hal_gpio_get_level(input_ppm->gpio);

    if (gpio_level == input_ppm->last_gpio_level)
        return false;

    input_ppm->last_gpio_level = gpio_level;

    if (gpio_level == HAL_GPIO_HIGH)
    {
        time_micros_t pulse_length = now - input_ppm->last_pulse;
        input_ppm->last_pulse = now;

        /* Sync pulse detection */
        if (pulse_length > PPM_IN_MIN_SYNC_PULSE_US)
        {
            if (input_ppm->pulseIndex == input_ppm->numChannelsPrevFrame
                && input_ppm->pulseIndex >= PPM_IN_MIN_NUM_CHANNELS
                && input_ppm->pulseIndex <= PPM_IN_MAX_NUM_CHANNELS)
            {
                /* If we see n simultaneous frames of the same
               number of channels we save it as our frame size */
                if (input_ppm->stableFramesSeenCount < PPM_STABLE_FRAMES_REQUIRED_COUNT)
                {
                    input_ppm->stableFramesSeenCount++;
                }
                else
                {
                    input_ppm->numChannels = input_ppm->pulseIndex;
                }
            }
            else
            {
                input_ppm->stableFramesSeenCount = 0;
            }

            /* Check if the last frame was well formed */
            if (input_ppm->pulseIndex == input_ppm->numChannels && input_ppm->tracking)
            {
                /* The last frame was well formed */
                for (i = 0; i < input_ppm->numChannels; i++)
                {
                    rc_data_update_channel(input_ppm->input.rc_data, i,
                                           PPM_VALUE_MAPPING(input_ppm->captures[i]), now);
                }
                for (i = input_ppm->numChannels; i < PPM_IN_MAX_NUM_CHANNELS; i++)
                {
                    rc_data_update_channel(input_ppm->input.rc_data, i,
                                           PPM_RCVR_TIMEOUT, now);
                }
                failsafe_reset_interval(&input_ppm->input.failsafe, now);

                updated = true;
            }

            input_ppm->tracking = true;
            input_ppm->numChannelsPrevFrame = input_ppm->pulseIndex;
            input_ppm->pulseIndex = 0;

            /* We rely on the supervisor to set captureValue to invalid
           if no valid frame is found otherwise we ride over it */
        }
        else if (input_ppm->tracking)
        {
            /* Valid pulse duration 0.75 to 2.5 ms*/
            if (pulse_length > PPM_IN_MIN_CHANNEL_PULSE_US
                && pulse_length < PPM_IN_MAX_CHANNEL_PULSE_US
                && input_ppm->pulseIndex < PPM_IN_MAX_NUM_CHANNELS)
            {
                input_ppm->captures[input_ppm->pulseIndex] = pulse_length;
                input_ppm->pulseIndex++;
            }
            else
            {
                /* Not a valid pulse duration */
                input_ppm->tracking = false;
                for (i = 0; i < PPM_CAPTURE_COUNT; i++)
                {
                    input_ppm->captures[i] = PPM_RCVR_TIMEOUT;
                }
            }
        }
    }

    return updated;
}

static void input_ppm_close(void *input, void *config)
{
}

void input_ppm_init(input_ppm_t *input)
{
    input->input.vtable = (input_vtable_t){
        .open = input_ppm_open,
        .update = input_ppm_update,
        .close = input_ppm_close,
    };
    input->pulseIndex = 0;
    input->numChannels = -1;
    input->numChannelsPrevFrame = -1;
    input->stableFramesSeenCount = 0;
    input->tracking = false;
}