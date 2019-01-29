#include <hal/log.h>

#include "io/gpio.h"

#include "rc/rc_data.h"

#include "util/time.h"

#include "input_ppm.h"

static const char *TAG = "PPM.Input";

#define PPM_VALUE_MAPPING(ch) RC_CHANNEL_MIN_VALUE + (ch - PPM_IN_MIN_CHANNEL_VALUE) * (RC_CHANNEL_MAX_VALUE - RC_CHANNEL_MIN_VALUE) / (PPM_IN_MAX_CHANNEL_VALUE - PPM_IN_MIN_CHANNEL_VALUE)
#define PPM_VALUE_MAP_AND_THRESHOLDING(ch) (ch < PPM_IN_MIN_CHANNEL_VALUE ? RC_CHANNEL_MIN_VALUE : (ch > PPM_IN_MAX_CHANNEL_VALUE ? RC_CHANNEL_MAX_VALUE : PPM_VALUE_MAPPING(ch)))

static void IRAM_ATTR ppm_handle_isr(void *arg)
{
    input_ppm_t *input_ppm = arg;
    time_micros_t now = time_micros_now();
    if (input_ppm->pulseCountInQueue < PPM_PULSE_QUEUE_SIZE)
    {
        input_ppm->pulse_queue[input_ppm->pulseCountInQueue] = now;
        input_ppm->pulseCountInQueue++;
    }
}

static bool input_ppm_open(void *input, void *config)
{
    input_ppm_t *input_ppm = input;
    input_ppm_config_t *config_ppm = config;
    input_ppm->gpio = config_ppm->gpio;
    HAL_ERR_ASSERT_OK(hal_gpio_setup(input_ppm->gpio, HAL_GPIO_DIR_INPUT, HAL_GPIO_PULL_UP));

    time_micros_t now = time_micros_now();
    failsafe_set_max_interval(&input_ppm->input.failsafe, MILLIS_TO_MICROS(200));
    failsafe_reset_interval(&input_ppm->input.failsafe, now);

    input_ppm->last_pulse = 0;
    input_ppm->pulseCountInQueue = 0;

    HAL_ERR_ASSERT_OK(hal_gpio_set_isr(input_ppm->gpio, HAL_GPIO_INTR_POSEDGE, ppm_handle_isr, input));

    LOG_I(TAG, "Open on GPIO %s", gpio_toa(input_ppm->gpio));

    return true;
}

static bool input_ppm_update(void *input, rc_data_t *data, time_micros_t now)
{
    input_ppm_t *input_ppm = input;
    int32_t i;
    bool updated = false;
    time_micros_t pulse_length = 0;
    time_micros_t current_pulse = 0;

    if (input_ppm->pulseCountInQueue == 0)
    {
        return false;
    }

    //Note: theoretically a MUTEX should be placed on dequeue operation.
    // Since the min inverval of two valid PPM pulses is at least 0.75 ms,
    // collision is unlikely to happen while processing a non-empty queue.
    // And, even collision happens, the worse result is that two PPM frames
    // been ignored.
    if (input_ppm->pulseCountInQueue > 0)
    {
        current_pulse = input_ppm->pulse_queue[0];

        for (i = 1; i < input_ppm->pulseCountInQueue; i++)
        {
            input_ppm->pulse_queue[i - 1] = input_ppm->pulse_queue[i];
        }
        input_ppm->pulseCountInQueue--;
    }

    //The following PPM process logic copied from betaflight
    pulse_length = current_pulse - input_ppm->last_pulse;
    input_ppm->last_pulse = current_pulse;
    if (pulse_length > 0)
    {
        /* Sync pulse detection */
        if (pulse_length > PPM_IN_MIN_SYNC_PULSE_US)
        {
            if (input_ppm->pulseIndex == input_ppm->numChannelsPrevFrame && input_ppm->pulseIndex >= PPM_IN_MIN_NUM_CHANNELS && input_ppm->pulseIndex <= PPM_IN_MAX_NUM_CHANNELS)
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
                                           PPM_VALUE_MAP_AND_THRESHOLDING(input_ppm->captures[i]), now);
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
            if (pulse_length > PPM_IN_MIN_CHANNEL_PULSE_US && pulse_length < PPM_IN_MAX_CHANNEL_PULSE_US && input_ppm->pulseIndex < PPM_IN_MAX_NUM_CHANNELS)
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
    input_ppm_t *input_ppm = input;
    hal_gpio_set_isr(input_ppm->gpio, HAL_GPIO_INTR_POSEDGE, NULL, NULL);
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