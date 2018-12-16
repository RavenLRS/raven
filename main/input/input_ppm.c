
#include "util/time.h"
#include <hal/gpio.h>
#include <hal/rand.h>

#include "input_ppm.h"

static bool input_ppm_open(void *input, void *config)
{
    input_ppm_t *input_ppm = input;
    HAL_ERR_ASSERT_OK(hal_gpio_setup(input->gpio, HAL_GPIO_DIR_INPUT, HAL_GPIO_PULL_DOWN));

    //HAL_ERR_ASSERT_OK(hal_gpio_set_isr(input->gpio, GPIO_INTR_POSEDGE , lora_handle_isr, sx127x));

    failsafe_set_max_interval(&input_ppm->input.failsafe, 1000);

    input->last_gpio_level = hal_gpio_get_level(input->input);
    
    /*
    time_micros_t now = time_micros_now();    
    for (int ii = 0; ii < PPM_INPUT_MOVED_CHANNELS; ii++)
    {
        input_ppm->channels[ii] = hal_rand_u32() % 2 == 0;
        rc_data_update_channel(input_ppm->input.rc_data, ii, RC_CHANNEL_CENTER_VALUE, now);
    }
    // Put all flipped channels to low, so they have a value
    for (int ii = PPM_INPUT_MOVED_CHANNELS; ii < RC_CHANNELS_NUM; ii++)
    {
        rc_data_update_channel(input_ppm->input.rc_data, ii, RC_CHANNEL_MIN_VALUE, now);
    }
    */
    input_ppm->last_pulse = 0;
    INPUT_SET_MSP_TRANSPORT(input_ppm, NULL);
    return true;
}

static void input_ppm_update_channel(input_ppm_t *input, rc_data_t *data, int idx, time_micros_t now)
{
    control_channel_t *ch = &data->channels[idx];
    unsigned value = ch->value;
#if !defined(INPUT_PPM_CONSTANT)
    bool *up = &input->channels[idx];
    if (*up && ch->value == RC_CHANNEL_MAX_VALUE)
    {
        *up = false;
    }
    else if (!(*up) && ch->value == RC_CHANNEL_MIN_VALUE)
    {
        *up = true;
    }
    value += *up ? 1 : -1;
#endif
    rc_data_update_channel(data, idx, value, now);
}

static bool input_ppm_update(void *input, rc_data_t *data, time_micros_t now)
{
    input_ppm_t *input_ppm = input;

    int gpio_level = hal_gpio_get_level(input_ppm->gpio);

    if (gpio_level == input_ppm->last_gpio_level)
        return false;

    if (gpio_level == HAL_GPIO_HIGH)
    {
        time_micros_t pulse_length = now - input->last_pulse;
        if( pulse_length > PPM_IN_MIN_SYNC_PULSE_US)
        {
            input->next_channel_index = 0;
        }
        else if ( pulse_length > PPM_IN_MIN_CHANNEL_PULSE_US 
            && pulse_length < PPM_IN_MAX_CHANNEL_PULSE_US 
            && input->next_channel_index != -1)
        {
            rc_data_update_channel(input_ppm->input.rc_data, input->next_channel_index,
              pulse_length, now);
        }
    }
    if (failsafe_is_active(&input_ppm->input.failsafe))
    {
        return false;
    }
    if (now > input_ppm->next_update)
    {
        for (int ii = 0; ii < PPM_INPUT_MOVED_CHANNELS; ii++)
        {
            input_ppm_update_channel(input_ppm, data, ii, now);
        }
        input_ppm->next_update = now + input_ppm->update_interval;

        // Once in a while, flip a random channel in the remaining channels
        if (now > input_ppm->next_flip)
        {
            int ch = PPM_INPUT_MOVED_CHANNELS + (hal_rand_u32() % (RC_CHANNELS_NUM - PPM_INPUT_MOVED_CHANNELS));
            unsigned value = data->channels[ch].value;
#if !defined(INPUT_PPM_CONSTANT)
            if (value > RC_CHANNEL_MAX_VALUE * 0.9)
            {
                // Channel high
                value = hal_rand_u32() % 2 == 0 ? RC_CHANNEL_MIN_VALUE : RC_CHANNEL_CENTER_VALUE;
            }
            else if (value < RC_CHANNEL_MAX_VALUE * 0.1)
            {
                // Channel low
                value = hal_rand_u32() % 2 == 0 ? RC_CHANNEL_CENTER_VALUE : RC_CHANNEL_MAX_VALUE;
            }
            else
            {
                // Channel mid
                value = hal_rand_u32() % 2 == 0 ? RC_CHANNEL_MIN_VALUE : RC_CHANNEL_MAX_VALUE;
            }
#endif
            rc_data_update_channel(data, ch, value, now);
            input_ppm->next_flip = now + MILLIS_TO_MICROS(500);
        }
        return true;
    }
    return false;
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
    input->next_channel_index = -1;
}